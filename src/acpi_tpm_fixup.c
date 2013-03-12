// Add ACPI SSDT containing the TPM Device for Chromebook Legacy mode
//
// Copyright 2013 Google Inc.
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "util.h"
#include "acpi.h"

/*
 * ACPI Common Header
 */
struct acpi_header         	/* ACPI common table header */
{
    ACPI_TABLE_HEADER_DEF
} PACKED;

/*
 * ACPI 1.0 Root System Description Table (RSDT)
 */
#define RSDT_SIGNATURE 0x54445352 // RSDT
#define RSDP_ADDR_START 0xf0000
#define RSDP_ADDR_MAX 0xfffff
struct rsdt_descriptor
{
    ACPI_TABLE_HEADER_DEF       /* ACPI common table header */
    u32 table_offset_entry[0];  /* Array of pointers to other */
} PACKED;

/*
 * ACPI Extended System Description Table (XSDT)
 */
#define XSDT_SIGNATURE 0x54445358 // XSDT
struct xsdt_descriptor
{
    ACPI_TABLE_HEADER_DEF       /* ACPI common table header */
    u64 table_offset_entry[0];  /* Array of pointers to other */
} PACKED;

/*
 * Secondary System Description Table (SSDT)
 */
#define SSDT_SIGNATURE 0x54445353 // SSDT

/*
 * DefinitionBlock ("", "SSDT", 1, "", "", 0x20130310)
 * {
 *   External (\_SB.PCI0.LPCB, DeviceObj)
 *   Scope (\_SB.PCI0.LPCB)
 *   {
 *     Device (TPM)
 *     {
 *       Name (_HID, EISAID("IFX0102"))
 *       Name (_CID, EISAID("PNP0C31"))
 *       Name (_UID, One)
 *
 *       Method (_STA, 0, NotSerialized)
 *       {
 *         Return (0x0F)
 *       }
 *
 *       Name (_CRS, ResourceTemplate() {
 *         IO (Decode16, 0x002E, 0x002E, 0x01, 0x02)
 *         IO (Decode16, 0x06F0, 0x06F0, 0x01, 0x10)
 *         Memory32Fixed (ReadWrite, 0xFED40000, 0x00005000)
 *       })
 *     }
 *   }
 * }
 */

static const u8 ssdt_tpm_aml[] = {
    0x53, 0x53, 0x44, 0x54, 0x89, 0x00, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x10, 0x03, 0x13, 0x20, 0x49, 0x4e, 0x54, 0x4c, 0x28, 0x05, 0x10, 0x20,
    0x10, 0x44, 0x06, 0x5c, 0x2f, 0x03, 0x5f, 0x53, 0x42, 0x5f, 0x50, 0x43,
    0x49, 0x30, 0x4c, 0x50, 0x43, 0x42, 0x5b, 0x82, 0x41, 0x05, 0x54, 0x50,
    0x4d, 0x5f, 0x08, 0x5f, 0x48, 0x49, 0x44, 0x0c, 0x24, 0xd8, 0x01, 0x02,
    0x08, 0x5f, 0x43, 0x49, 0x44, 0x0c, 0x41, 0xd0, 0x0c, 0x31, 0x08, 0x5f,
    0x55, 0x49, 0x44, 0x01, 0x14, 0x09, 0x5f, 0x53, 0x54, 0x41, 0x00, 0xa4,
    0x0a, 0x0f, 0x08, 0x5f, 0x43, 0x52, 0x53, 0x11, 0x21, 0x0a, 0x1e, 0x47,
    0x01, 0x2e, 0x00, 0x2e, 0x00, 0x01, 0x02, 0x47, 0x01, 0xf0, 0x06, 0xf0,
    0x06, 0x01, 0x10, 0x86, 0x09, 0x00, 0x01, 0x00, 0x00, 0xd4, 0xfe, 0x00,
    0x50, 0x00, 0x00, 0x79, 0x00
};
static const u8 ssdt_tpm_aml_len = 137;

void
acpi_tpm_fixup(void)
{
    struct acpi_header *ssdt_header = NULL;
    struct rsdp_descriptor *rsdp = NULL;
    struct rsdt_descriptor *rsdt;
    struct xsdt_descriptor *xsdt;
    void *p = (void*)RSDP_ADDR_START;
    void *end = (void*)RSDP_ADDR_MAX;
    void *ssdt_data;
    int i;

    /* Only apply this fixup on the Chromebook Pixel (aka GOOGLE Link) */
    if (strcmp(CBvendor, "GOOGLE") != 0 || strcmp(CBpart, "Link") != 0)
        return;

    /* Find the RSDP */
    for (; p<end; p += 16) {
        rsdp = p;
        if (rsdp && rsdp->signature == RSDP_SIGNATURE)
            break;
    }
    if (!rsdp || rsdp->signature != RSDP_SIGNATURE)
        return;

    /* Find the RSDT */
    rsdt = (void*)rsdp->rsdt_physical_address;
    if (!rsdt || rsdt->signature != RSDT_SIGNATURE)
        return;
    end = (void*)rsdt + rsdt->length;

    /* Find the existing SSDT in the RSDT */
    for (i=0; (void*)&rsdt->table_offset_entry[i] < end; i++) {
        ssdt_header = (void*)rsdt->table_offset_entry[i];
        if (ssdt_header && ssdt_header->signature == SSDT_SIGNATURE)
            break;
    }
    if (!ssdt_header || ssdt_header->signature != SSDT_SIGNATURE)
        return;

    /* Create the new SSDT after the existing one */
    ssdt_data = (void*)ALIGN((u32)ssdt_header + ssdt_header->length, 16);
    memcpy(ssdt_data, ssdt_tpm_aml, ssdt_tpm_aml_len);

    /* Add this entry to the RSDT */
    rsdt->table_offset_entry[i] = (u32)ssdt_data;
    rsdt->length += sizeof(u32);
    rsdt->checksum = 0;
    rsdt->checksum -= checksum(rsdt, rsdt->length);

    /* Find free entry in the XSDT */
    xsdt = (void*)(u32)rsdp->xsdt_physical_address;
    if (!xsdt || xsdt->signature != XSDT_SIGNATURE)
        return;
    end = (void*)xsdt + xsdt->length;

    /* Find the first empty entry */
    for (i=0; (void*)&xsdt->table_offset_entry[i] < end; i++);

    /* Add this entry to the XSDT */
    xsdt->table_offset_entry[i] = (u32)ssdt_data;
    xsdt->length += sizeof(u64);
    xsdt->checksum = 0;
    xsdt->checksum -= checksum(xsdt, xsdt->length);
}

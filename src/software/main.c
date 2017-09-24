#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#include <fpga_pci.h>
#include <fpga_mgmt.h>
#include <utils/lcd.h>

#define INPUT_A0_BASE UINT64_C(0x004000)
#define INPUT_A1_BASE UINT64_C(0x002000)
#define INPUT_A2_BASE UINT64_C(0x003000)
#define INPUT_A3_BASE UINT64_C(0x004000)
#define INPUT_A4_BASE UINT64_C(0x005000)
#define INPUT_A5_BASE UINT64_C(0x010000)
#define INPUT_A6_BASE UINT64_C(0x000000)
#define INPUT_A7_BASE UINT64_C(0x001000)

#define INPUT_B0_BASE UINT64_C(0x007000)
#define INPUT_B1_BASE UINT64_C(0x008000)
#define INPUT_B2_BASE UINT64_C(0x00C000)
#define INPUT_B3_BASE UINT64_C(0x00A000)
#define INPUT_B4_BASE UINT64_C(0x00D000)
#define INPUT_B5_BASE UINT64_C(0x009000)
#define INPUT_B6_BASE UINT64_C(0x00E000)
#define INPUT_B7_BASE UINT64_C(0x00B000)

#define OUTPUT_BASE UINT64_C(0x00F000)

static uint16_t pci_vendor_id = 0x1D0F; /* Amazon PCI Vendor ID */
static uint16_t pci_device_id = 0xF000; /* PCI Device ID preassigned by Amazon for F1 applications */

int check_afi_ready(int slot_id);

int main(int argc, char *argv[])
{
    int rc;

    rc = fpga_pci_init();
    fail_on(rc, out, "Unable to initialize the fpga_pci library");

    rc = check_afi_ready(0);
    fail_on(rc, out, "AFI not ready");

    // Attach PCIe
    pci_bar_handle_t pci_bar_handle = PCI_BAR_HANDLE_INIT;
    rc = fpga_pci_attach(0, FPGA_APP_PF, APP_PF_BAR1, BURST_CAPABLE, &pci_bar_handle);
    fail_on(rc, out, "Unable to attach to the AFI on slot id %d", 0);


    // DDR TEST

    uint32_t a[8];
    uint32_t b[8];

    // Read inputs for A
    int value;
    for (int i = 0; i < 8; i++) {
        printf("%2d> ", i+1);
        scanf("%d", &value);
        a[i] = (uint32_t) value;
    }

    // Read inputs for B
    for (int i = 0; i < 8; i++) {
        printf("%2d> ", i+1);
        scanf("%d", &value);
        b[i] = (uint32_t) value;
    }

    printf("Expected result for: ");
    int expected = 0;
    for (int i = 0; i < 8; i++) {
        printf("%d * %d", (int) a[i], (int) b[i]);
        if (i < 7) {
            printf(" + ");
        }

        expected += (int) a[i] * (int) b[i];
    }
    printf(" = %d\n", expected);

    // Write inputs
    uint64_t a_addrs[8] = {
        INPUT_A0_BASE,
        INPUT_A1_BASE,
        INPUT_A2_BASE,
        INPUT_A3_BASE,
        INPUT_A4_BASE,
        INPUT_A5_BASE,
        INPUT_A6_BASE,
        INPUT_A7_BASE
    };
    uint64_t b_addrs[8] = {
        INPUT_B0_BASE,
        INPUT_B1_BASE,
        INPUT_B2_BASE,
        INPUT_B3_BASE,
        INPUT_B4_BASE,
        INPUT_B5_BASE,
        INPUT_B6_BASE,
        INPUT_B7_BASE
    };

    for (int i = 0; i < 8; i++) {
        rc = fpga_pci_poke(pci_bar_handle, a_addrs[i], a[i]);
        rc = fpga_pci_poke(pci_bar_handle, b_addrs[i], b[i]);
        fail_on(rc, out, "Unable to write to FPGA");
    }

    uint32_t *result = malloc(sizeof(uint32_t));
    rc = fpga_pci_peek(pci_bar_handle, OUTPUT_BASE, result);
    printf("Result: %d\n", (int) *result);

out:
    if (pci_bar_handle >= 0) {
        rc = fpga_pci_detach(pci_bar_handle);
        if (rc) {
            printf("Failure while detaching from FPGA.\n");
        }
    }

    return (rc != 0 ? 1 : 0);
}

int check_afi_ready(int slot_id)
{
    struct fpga_mgmt_image_info info = {0};
    int rc;

    /* get local image description, contains status, vendor id, and device id. */
    rc = fpga_mgmt_describe_local_image(slot_id, &info,0);
    fail_on(rc, out, "Unable to get AFI information from slot %d. Are you running as root?",slot_id);

    /* check to see if the slot is ready */
    if (info.status != FPGA_STATUS_LOADED) {
        rc = 1;
        fail_on(rc, out, "AFI in Slot %d is not in READY state !", slot_id);
    }

    printf("AFI PCI  Vendor ID: 0x%x, Device ID 0x%x\n",
        info.spec.map[FPGA_APP_PF].vendor_id,
        info.spec.map[FPGA_APP_PF].device_id);

    /* confirm that the AFI that we expect is in fact loaded */
    if (info.spec.map[FPGA_APP_PF].vendor_id != pci_vendor_id ||
        info.spec.map[FPGA_APP_PF].device_id != pci_device_id) {
        printf("AFI does not show expected PCI vendor id and device ID. If the AFI "
               "was just loaded, it might need a rescan. Rescanning now.\n");

        rc = fpga_pci_rescan_slot_app_pfs(slot_id);
        fail_on(rc, out, "Unable to update PF for slot %d",slot_id);
        /* get local image description, contains status, vendor id, and device id. */
        rc = fpga_mgmt_describe_local_image(slot_id, &info,0);
        fail_on(rc, out, "Unable to get AFI information from slot %d",slot_id);

        printf("AFI PCI  Vendor ID: 0x%x, Device ID 0x%x\n",
            info.spec.map[FPGA_APP_PF].vendor_id,
            info.spec.map[FPGA_APP_PF].device_id);

        /* confirm that the AFI that we expect is in fact loaded after rescan */
        if (info.spec.map[FPGA_APP_PF].vendor_id != pci_vendor_id ||
             info.spec.map[FPGA_APP_PF].device_id != pci_device_id) {
            rc = 1;
            fail_on(rc, out, "The PCI vendor id and device of the loaded AFI are not "
                             "the expected values.");
        }
    }

    return rc;

out:
    return 1;
}

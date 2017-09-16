#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <fpga_pci.h>
#include <fpga_mgmt.h>
#include <utils/lcd.h>

#define IN_BUF_SIZE 16
#define OUT_BUF_SIZE 32

// Memory address offsets
#define INPUT_1_OFFSET UINT64_C(0x00010000)
#define INPUT_2_OFFSET UINT64_C(0x00001000)
#define OUTPUT_OFFSET UINT64_C(0x00002000)

static uint16_t pci_vendor_id = 0x1D0F; /* Amazon PCI Vendor ID */
static uint16_t pci_device_id = 0xF000; /* PCI Device ID preassigned by Amazon for F1 applications */

int check_afi_ready(int slot_id);
int multiply(int a, int b, uint32_t *result);

int main(int argc, char *argv[])
{
    int rc;

    rc = fpga_pci_init();
    fail_on(rc, out, "Unable to initialize the fpga_pci library");

    rc = check_afi_ready(0);
    fail_on(rc, out, "AFI not ready");

    int a = atoi(argv[1]);
    int b = atoi(argv[2]);
    uint32_t result;

    printf("Expecting: %d * %d = %d\n", a, b, a*b);
    rc = multiply(a, b, &result);
    fail_on(rc, out, "Multiplication failed");
    printf("Actual: %d\n", result);

    return rc;

out:
    return 1;
}

int multiply(int a, int b, uint32_t *result)
{
    int rc;
    pci_bar_handle_t pci_bar_handle = PCI_BAR_HANDLE_INIT;

    rc = fpga_pci_attach(0, FPGA_APP_PF, APP_PF_BAR4, 0, &pci_bar_handle);
    fail_on(rc, out, "Unable to attach to the AFI on slot id %d", 0);

    rc = fpga_pci_poke(pci_bar_handle, INPUT_1_OFFSET, a);
    fail_on(rc, out, "Unable to write to FPGA");
    rc = fpga_pci_poke(pci_bar_handle, INPUT_2_OFFSET, b);
    fail_on(rc, out, "Unable to write to FPGA");

    rc = fpga_pci_peek(pci_bar_handle, OUTPUT_OFFSET, result);
    fail_on(rc, out, "Unalbe to read from the FPGA");

    return rc;

out:
    if (pci_bar_handle >= 0) {
        rc = fpga_pci_detach(pci_bar_handle);
        if (rc) {
            printf("Failure while detaching from the FPGA.\n");
        }
    }

    return (rc != 0 ? 1 : 0);
}


int check_afi_ready(int slot_id) {
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

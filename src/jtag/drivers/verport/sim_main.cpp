// DESCRIPTION: Verilator: Verilog example module
//
// This file ONLY is placed into the Public Domain, for any use,
// without warranty, 2017 by Wilson Snyder.
//======================================================================

// Include common routines
#include <verilated.h>
#include "Vtap_ext.h"

#include <sys/stat.h>  // mkdir

// Include model header, generated from Verilating "top.v"
#include "Vglbl.h"

// If "verilator --trace" is used, include the tracing class
#if VM_TRACE
# include <verilated_vcd_c.h>
#endif

// Current simulation time (64-bit unsigned)
vluint64_t main_time = 0;
// Called by $time in Verilog
double sc_time_stamp () {
    return main_time; // Note does conversion to real, to match SystemC
}

Vglbl* top;
VerilatedVcdC* tfp;

extern "C" void Vtap_start(void) {
  const char *argv[] = {"a.out", NULL};
  
    // This is a more complicated example, please also see the simpler examples/hello_world_c.

    // Pass arguments so Verilated code can see them, e.g. $value$plusargs
    Verilated::commandArgs(1, argv);
    
    // Set debug level, 0 is off, 9 is highest presently used
    Verilated::debug(0);

    // Randomization reset policy
    Verilated::randReset(2);

    // Construct the Verilated model, from Vglbl.h generated from Verilating "top.v"
    top = new Vglbl; // Or use a const unique_ptr, or the VL_UNIQUE_PTR wrapper

    // If verilator was invoked with --trace, open trace
#if VM_TRACE
    Verilated::traceEverOn(true);  // Verilator must compute traced signals
    VL_PRINTF("Enabling waves into logs/vlt_dump.vcd...\n");
    tfp = new VerilatedVcdC;
    top->trace(tfp, 99);  // Trace 99 levels of hierarchy
    mkdir("logs", 0777);
    tfp->open("logs/vlt_dump.vcd");  // Open the dump file
#endif

}

extern "C" int Vtap_time_step(int tms, int tck, int trstn, int tdi)
{
  static int old_ir, old_led;
  
        top->clk_p = 1 & (main_time++);  // Time passes...

        top->tms_pad_i = tms;
        top->tck_pad_i = tck;
        top->trstn_pad_i = trstn;
        top->tdi_pad_i = tdi;
        top->rst_top = trstn;
        
        // Evaluate model
        top->eval();

#if VM_TRACE
    if (tfp) { tfp->dump(main_time); }
#endif
        // Read outputs
        /*
        VL_PRINTF ("[%" VL_PRI64 "d] tck_pad_i=%x tms_pad_i=%x tdi_pad_i=%x tdo_pad_o=%x rstl=%x\n",
                   main_time, top->tck_pad_i, top->tms_pad_i, top->tdi_pad_i, top->tdo_pad_o, top->trstn_pad_i);

        VL_PRINTF ("[%" VL_PRI64 "d] reset=%x run_test=%x shift=%x pause=%x update=%x capture=%x\n",
                   main_time, top->test_logic_reset_o, top->run_test_idle_o, top->shift_dr_o, top->pause_dr_o,
                   top->update_dr_o, top->capture_dr_o);
        */

        if (old_ir != top->latched_jtag_ir)
          {
            VL_PRINTF ("[%" VL_PRI64 "d] ir=%x\n", main_time, top->latched_jtag_ir);
            old_ir = top->latched_jtag_ir;
          }

        if (old_led != top->o_led)
          {
            VL_PRINTF ("[%" VL_PRI64 "d] ir=%x\n", main_time, top->o_led);
            old_led = top->o_led;
          }

        return top->tdo_pad_o;
    }

extern "C" void Vtap_finish(void)
{
    // Final model cleanup
    top->final();

    // Close trace if opened
#if VM_TRACE
    if (tfp) { tfp->close(); }
#endif

    //  Coverage analysis (since test passed)
#if VM_COVERAGE
    mkdir("logs", 0777);
    VerilatedCov::write("logs/coverage.dat");
#endif

    // Destroy model
    delete top; top = NULL;

    // Fin
    exit(0);
}

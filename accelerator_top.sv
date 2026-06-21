//////////////////////////////////////////////////////////////////////
////  accelerator_top.sv -- modified for 8-element dot-product    ////
////  ILA wrapped in `ifndef SIMULATION so it's active on FPGA    ////
////  but skipped during XSim behavioral simulation               ////
////  Alex Grinshpun 2023                                         ////
////  Provided AS IS without any warranty of any kind nor EXPLICIT nor IMPLIED
//////////////////////////////////////////////////////////////////////

module accelerator_top (

    input                       wb_clk_i,

    // WISHBONE interface
    input   logic               wb_rst_i,

    input   logic               wb_stb_i,
    input   logic   [2:0]       wb_cti_i,
    input   logic   [1:0]       wb_bte_i,
    input   logic               wb_cyc_i,
    input   logic   [3:0]       wb_sel_i,
    input   logic               wb_we_i,
    input   logic   [7:0]       wb_adr_i,
    input   logic   [31:0]      wb_dat_i,
    output  logic   [31:0]      wb_dat_o,

    output  logic               wb_ack_o,
    output  logic               wb_err_o,
    output  logic               wb_rty_o,
    output  logic               int_o
);

parameter SIM   = 0;
parameter debug = 0;

logic   [31:0]  wb_data_reg_out;
logic   [31:0]  wb_data_reg_in;
logic   [7:0]   wb_adr_int;
logic           we_o;
logic           re_o;

// Operand and result registers shared between regs and core
logic   [31:0]  reg_a;
logic   [31:0]  reg_b;
logic   [31:0]  reg_c;
logic   [31:0]  reg_d;
logic   [31:0]  reg_result;

// Control handshake between regs and core
logic           go;
logic           done_set;

assign int_o = 1'b0;

//-------------------------------------------------------------------------
// ILA -- Integrated Logic Analyzer for in-system debug.
// Active for FPGA synthesis; skipped during simulation (SIMULATION macro
// is defined in Project Settings -> Simulation -> Compilation as
// xsim.compile.xvlog.more_options = "-d SIMULATION").
//
// Configure the ila_matrix IP with 10 probes of these widths:
//   probe0:1,  probe1:1,  probe2:32, probe3:32, probe4:32,
//   probe5:32, probe6:32, probe7:8,  probe8:1,  probe9:1
//-------------------------------------------------------------------------
//`ifndef SIMULATION
//ila_matrix ila_matrix (
//    .clk    (wb_clk_i   ),
//    .probe0 (go         ),     // [0]    start pulse  -> use as trigger
//    .probe1 (done_set   ),     // [0]    completion pulse
//    .probe2 (reg_a      ),     // [31:0] vector A (packed bytes)
//    .probe3 (reg_b      ),     // [31:0] vector B (packed bytes)
//    .probe4 (reg_c      ),     // [31:0] vector C (packed bytes)
//    .probe5 (reg_d      ),     // [31:0] vector D (packed bytes)
//    .probe6 (reg_result ),     // [31:0] computed dot product
//    .probe7 (wb_adr_int ),     // [7:0]  internal bus address
//    .probe8 (we_o       ),     // [0]    write enable
//    .probe9 (re_o       )      // [0]    read enable
//);
//`endif

//-------------------------------------------------------------------------
// MODULE INSTANCES
//-------------------------------------------------------------------------

// WISHBONE interface (unchanged)
accelerator_wb wb_interface (
    .clk             (wb_clk_i        ),
    .wb_rst_i        (wb_rst_i        ),
    .wb_we_i         (wb_we_i         ),
    .wb_stb_i        (wb_stb_i        ),
    .wb_cti_i        (wb_cti_i        ),
    .wb_bte_i        (wb_bte_i        ),
    .wb_cyc_i        (wb_cyc_i        ),
    .wb_ack_o        (wb_ack_o        ),
    .wb_sel_i        (4'b0            ),
    .wb_adr_i        (wb_adr_i        ),
    .wb_dat_i        (wb_dat_i        ),
    .wb_dat_o        (wb_dat_o        ),
    .wb_err_o        (wb_err_o        ),
    .wb_rty_o        (wb_rty_o        ),
    .wb_adr_reg      (wb_adr_int      ),
    .wb_data_reg_in  (wb_data_reg_in  ),
    .wb_data_reg_out (wb_data_reg_out ),
    .we_o            (we_o            ),
    .re_o            (re_o            )
);

// Register file (Control, A, B, C, D, Result)
accelerator_regs regs (
    .clk        (wb_clk_i        ),
    .wb_rst_i   (wb_rst_i        ),
    .wb_addr_i  (wb_adr_int      ),
    .wb_dat_i   (wb_data_reg_out ),
    .wb_dat_o   (wb_data_reg_in  ),
    .wb_we_i    (we_o            ),
    .wb_re_i    (re_o            ),
    .reg_a      (reg_a           ),
    .reg_b      (reg_b           ),
    .reg_c      (reg_c           ),
    .reg_d      (reg_d           ),
    .reg_result (reg_result      ),
    .go         (go              ),
    .done_set   (done_set        )
);

// Dot-product core
accelerator accelerator (
    .clk        (wb_clk_i  ),
    .wb_rst_i   (wb_rst_i  ),
    .reg_a      (reg_a     ),
    .reg_b      (reg_b     ),
    .reg_c      (reg_c     ),
    .reg_d      (reg_d     ),
    .go         (go        ),
    .done_set   (done_set  ),
    .reg_result (reg_result)
);

endmodule
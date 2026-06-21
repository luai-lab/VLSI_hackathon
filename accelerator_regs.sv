// Alex Grinshpun Dec 2023 -- modified for 8-element dot-product
// Register addresses (offsets from base 0x80001300)
// Provided AS IS without any warranty of any kind nor EXPLICIT nor IMPLIED

`define ACCELERATOR_REG_CONTROL     8'h00    // 0x80001300 : Control (bit0=Go, bit31=Done)
`define ACCELERATOR_REG_A           8'h04    // 0x80001304 : Vector A (4 x 8-bit elements)
`define ACCELERATOR_REG_B           8'h08    // 0x80001308 : Vector B (4 x 8-bit elements)
`define ACCELERATOR_REG_C           8'h0C    // 0x8000130C : Vector C (4 x 8-bit elements)
`define ACCELERATOR_REG_D           8'h10    // 0x80001310 : Vector D (4 x 8-bit elements)
`define ACCELERATOR_REG_RESULT      8'h14    // 0x80001314 : Result (signed 32-bit)


module accelerator_regs
#(parameter SIM = 0)
(
    input   logic                   clk,
    input   logic                   wb_rst_i,
    input   logic   [7:0]           wb_addr_i,
    input   logic   [31:0]          wb_dat_i,
    output  logic   [31:0]          wb_dat_o,
    input   logic                   wb_we_i,
    input   logic                   wb_re_i,

    // To/from the accelerator core
    output  logic   [31:0]          reg_a,        // packed A[3:0]
    output  logic   [31:0]          reg_b,        // packed B[3:0]
    output  logic   [31:0]          reg_c,        // packed C[3:0]
    output  logic   [31:0]          reg_d,        // packed D[3:0]
    input   logic   [31:0]          reg_result,   // signed result from core

    output  logic                   go,           // 1-cycle pulse: start the core
    input   logic                   done_set      // 1-cycle pulse from core: latch Done=1
);

logic           done_q;          // sticky Done bit (cleared on next Go)

//-------------------------------------------------------------------------
// Asynchronous read port
//-------------------------------------------------------------------------
always_comb begin
    case (wb_addr_i)
        `ACCELERATOR_REG_CONTROL : wb_dat_o = {done_q, 31'h0};   // bit31=Done, bit0=Go (always reads 0)
        `ACCELERATOR_REG_A       : wb_dat_o = reg_a;
        `ACCELERATOR_REG_B       : wb_dat_o = reg_b;
        `ACCELERATOR_REG_C       : wb_dat_o = reg_c;
        `ACCELERATOR_REG_D       : wb_dat_o = reg_d;
        `ACCELERATOR_REG_RESULT  : wb_dat_o = reg_result;
        default                  : wb_dat_o = 32'b0;
    endcase
end

//-------------------------------------------------------------------------
// Writes, Go pulse, and Done flag
//-------------------------------------------------------------------------
// Go is asserted for exactly one cycle when SW writes 1 to bit0 of Control.
// Writing to Control with bit0=1 also clears the Done flag so the next
// computation starts from "not done".

always_ff @(posedge clk or posedge wb_rst_i) begin
    if (wb_rst_i) begin
        reg_a   <= '0;
        reg_b   <= '0;
        reg_c   <= '0;
        reg_d   <= '0;
        go      <= 1'b0;
        done_q  <= 1'b0;
    end
    else begin
        // Default: Go is a single-cycle pulse
        go <= 1'b0;

        // Latch Done when the core signals completion
        if (done_set)
            done_q <= 1'b1;

        if (wb_we_i) begin
            case (wb_addr_i)
                `ACCELERATOR_REG_A       : reg_a <= wb_dat_i;
                `ACCELERATOR_REG_B       : reg_b <= wb_dat_i;
                `ACCELERATOR_REG_C       : reg_c <= wb_dat_i;
                `ACCELERATOR_REG_D       : reg_d <= wb_dat_i;
                `ACCELERATOR_REG_CONTROL : begin
                    if (wb_dat_i[0]) begin
                        go     <= 1'b1;     // start the accelerator
                        done_q <= 1'b0;     // clear Done for the new computation
                    end
                end
                default: ;  // ignore writes to Result and unmapped addresses
            endcase
        end
    end
end

endmodule
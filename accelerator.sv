// Alex Grinshpun Dec 2023 -- modified for 8-element dot-product
// Provided AS IS without any warranty of any kind nor EXPLICIT nor IMPLIED
//
// Computes:
//   Result = A[3]*B[3] + A[2]*B[2] + A[1]*B[1] + A[0]*B[0]
//          + C[3]*D[3] + C[2]*D[2] + C[1]*D[1] + C[0]*D[0]
//
// Each vector element is a signed 8-bit value packed into a 32-bit register
// in little-endian order: reg_a = {A[3], A[2], A[1], A[0]} (byte 3 .. byte 0).
//
// Protocol:
//   - SW writes the four operand registers (A, B, C, D).
//   - SW writes 1 to Control[0] (Go).
//   - The accelerator computes the dot product and pulses done_set on the
//     next clock edge. The regs module latches Done=1 for SW to poll.

module accelerator
(
    input   logic                       clk,
    input   logic                       wb_rst_i,

    // Operand vectors (each is 4 packed signed bytes)
    input   logic           [31:0]      reg_a,
    input   logic           [31:0]      reg_b,
    input   logic           [31:0]      reg_c,
    input   logic           [31:0]      reg_d,

    // Control / status
    input   logic                       go,         // 1-cycle start pulse
    output  logic                       done_set,   // 1-cycle "result ready" pulse

    // Result
    output  logic   signed  [31:0]      reg_result
);

logic           rst_n;
assign          rst_n = ~wb_rst_i;

//-------------------------------------------------------------------------
// Unpack each 32-bit register into 4 signed 8-bit elements.
// Element 0 = bits [7:0], element 1 = bits [15:8], etc.
//-------------------------------------------------------------------------
logic signed [7:0] a [0:3];
logic signed [7:0] b [0:3];
logic signed [7:0] c [0:3];
logic signed [7:0] d [0:3];

genvar gi;
generate
    for (gi = 0; gi < 4; gi++) begin : g_unpack
        assign a[gi] = reg_a[gi*8 +: 8];
        assign b[gi] = reg_b[gi*8 +: 8];
        assign c[gi] = reg_c[gi*8 +: 8];
        assign d[gi] = reg_d[gi*8 +: 8];
    end
endgenerate

//-------------------------------------------------------------------------
// Combinational dot product.
// Each signed 8x8 product fits in 16 bits; summing 8 of them needs at most
// 16 + ceil(log2(8)) = 19 bits. We compute in 32-bit signed arithmetic so
// the sign-extension and final truncation are straightforward.
//-------------------------------------------------------------------------
logic signed [31:0] dot_product;

always_comb begin
    dot_product = 32'sd0;
    for (int i = 0; i < 4; i++) begin
        dot_product += 32'(a[i]) * 32'(b[i]);
        dot_product += 32'(c[i]) * 32'(d[i]);
    end
end

//-------------------------------------------------------------------------
// Latch the result and signal completion when Go is asserted.
// Single-cycle compute: on the cycle after `go`, reg_result holds the
// new value and done_set pulses high for one cycle.
//-------------------------------------------------------------------------
always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
        reg_result <= 32'sd0;
        done_set   <= 1'b0;
    end
    else begin
        done_set <= 1'b0;            // default: not done
        if (go) begin
            reg_result <= dot_product;
            done_set   <= 1'b1;      // pulse: result is ready next cycle
        end
    end
end

endmodule
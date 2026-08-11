#include <iostream>
#include <bitset>
#include <cmath>
#include <string>
#include <iomanip>

using namespace std;

// Structure to hold the result of one clock cycle for printing
struct TickResult
{
    int p_out;
    string x_j_str;
    string y_j_str;
    string v_j_str;
    string w_j_str;
};

template <size_t N>
class OnlineMultiplier
{
private:
    bitset<N> Reg_WS;
    bitset<N> Reg_WC;

    bitset<N> Reg_X;
    bitset<N> Reg_Y;

    int x_delayed; // x_{j+4}
    int y_delayed; // y_{j+4}

    int append_index; // Tracks the current fractional bit position

    // ==========================================
    // HELPER: Dynamic 2's Complement Appending
    // ==========================================
    // Appends a digit (1, 0, -1) to the operand register dynamically.
    void append_digit(bitset<N> &reg, int digit, int pos)
    {
        if (digit == 1)
        {
            reg.set(pos);
        }
        else if (digit == -1)
        {
            // Subtracting 2^pos dynamically converts the trailing bits to 2's complement
            unsigned long long val = reg.to_ullong();
            val -= (1ULL << pos);
            reg = bitset<N>(val);
        }
    }

    // ==========================================
    // HELPER: String Formatter for Table Display
    // ==========================================
    string format_bits(const bitset<N> &reg, int start_idx, int end_idx, bool include_sign) const
    {
        string res = "";
        if (include_sign)
        {
            res += (reg[N - 1] ? "1" : "0");
            res += (reg[N - 2] ? "1" : "0");
        }
        res += ".";
        for (int i = start_idx; i >= end_idx; --i)
        {
            res += (reg[i] ? "1" : "0");
        }
        return res;
    }

public:
    OnlineMultiplier()
    {
        Reg_WS.reset();
        Reg_WC.reset();
        Reg_X.reset();
        Reg_Y.reset();
        x_delayed = 0;
        y_delayed = 0;
        append_index = N - 3;
    }

    // ==========================================
    // PHASE 2: Combinational Logic (Stateless)
    // ==========================================
    bitset<N> selector(const bitset<N> &operand, int digit) const
    {
        if (digit == 1)
            return operand;
        else if (digit == -1)
            return ~operand;
        else
            return bitset<N>(0);
    }

    void adder_4to2(const bitset<N> &in1, const bitset<N> &in2,
                    const bitset<N> &in3, const bitset<N> &in4,
                    bool c_x, bool c_y, int append_pos,
                    bitset<N> &out_vS, bitset<N> &out_vC) const
    {

        bitset<N> w = in1 ^ in2 ^ in3;
        bitset<N> c_out = (in1 & in2) | (in2 & in3) | (in1 & in3);
        bitset<N> c_in = c_out << 1;

        if (c_x)
            c_in.set(append_pos);

        out_vS = w ^ in4 ^ c_in;
        out_vC = (w & in4) | (in4 & c_in) | (w & c_in);

        if (c_y)
            out_vC.set(append_pos);
    }

    // ==========================================
    // PHASE 3: Control Logic & Estimation
    // ==========================================
    int v_block(const bitset<N> &vS, const bitset<N> &vC) const
    {
        int val_S = 0, val_C = 0;
        for (int i = 0; i < 4; ++i)
        {
            if (vS[N - 4 + i])
                val_S |= (1 << i);
            if (vC[N - 4 + i])
                val_C |= (1 << i);
        }
        int v_hat_raw = (val_S + val_C) & 0xF;
        if (v_hat_raw & 8)
            v_hat_raw |= ~7;
        return v_hat_raw;
    }

    int selm_block(int v_hat) const
    {
        if (v_hat >= 2)
            return 1;
        else if (v_hat <= -3)
            return -1;
        else
            return 0;
    }

    void m_block_and_shift(const bitset<N> &vS, const bitset<N> &vC, int p_out, int v_hat)
    {
        bool v_0 = (v_hat & 4) != 0;
        bool v_1 = (v_hat & 2) != 0;
        bool v_2 = (v_hat & 1) != 0;

        bool v_0_star = v_0 ^ (abs(p_out) == 1);

        Reg_WS = vS << 1;
        Reg_WC = vC << 1;

        Reg_WS[N - 1] = v_0_star;
        Reg_WS[N - 2] = v_1;
        Reg_WS[N - 3] = v_2;

        Reg_WC[N - 1] = 0;
        Reg_WC[N - 2] = 0;
        Reg_WC[N - 3] = 0;
    }

    // ==========================================
    // PHASE 4: Clock Engine (Tick)
    // ==========================================
    TickResult tick(int x_in, int y_in)
    {
        TickResult result;

        // 1. Append new digits to form y[j+1] (Before multiplication as per the math trick)
        append_digit(Reg_Y, y_in, append_index);

        // 2. Selectors (Multiply parallel vectors by incoming single digits)
        bitset<N> X_sel = selector(Reg_X, y_in);
        bitset<N> Y_sel = selector(Reg_Y, x_in);

        // Apply online delay delta=3 (Shift right by 3 bits mathematically)
        X_sel >>= 3;
        Y_sel >>= 3;

        // 3. [4:2] Compressor
        bitset<N> vS, vC;
        // If operand was -1, the selector did NOT (~). We must add +1 at the LSB position.
        // LSB is at (append_index - 3) due to the delta shift above.
        bool cx = (y_in == -1);
        bool cy = (x_in == -1);
        adder_4to2(Reg_WS, Reg_WC, X_sel, Y_sel, cx, cy, append_index - 3, vS, vC);

        // 4 & 5. Estimate v and Select Output Digit
        int v_hat = v_block(vS, vC);
        int p_out = selm_block(v_hat);

        // --- Prepare Display Strings (Simulation of exact math for printing) ---
        // Calculate exact v[j] = vS + vC
        unsigned long long exact_v = vS.to_ullong() + vC.to_ullong();
        bitset<N> v_val(exact_v);

        // Calculate exact w[j+1] = v[j] - p_{j+1}
        unsigned long long exact_w = exact_v;
        if (p_out == 1)
            exact_w -= (1ULL << (N - 2)); // Subtract 1.0
        else if (p_out == -1)
            exact_w += (1ULL << (N - 2)); // Add 1.0
        bitset<N> w_val(exact_w);

        // Append to X to form x[j+1] for the NEXT cycle
        append_digit(Reg_X, x_in, append_index);

        // Save strings before updating hardware state
        result.p_out = p_out;
        result.y_j_str = format_bits(Reg_Y, N - 3, append_index, false);
        result.x_j_str = format_bits(Reg_X, N - 3, append_index, false);
        result.v_j_str = format_bits(v_val, N - 3, append_index - 3, true);
        result.w_j_str = format_bits(w_val, N - 3, append_index - 3, true);

        // 6. Sign Correct and Shift (Hardware Update)
        m_block_and_shift(vS, vC, p_out, v_hat);

        // Move to the next fractional position for the next clock cycle
        append_index--;
        return result;
    }
};

int main()
{
    // 24 bits is safe to avoid overflow in our ullong conversions for this 8-digit test
    OnlineMultiplier<24> mult;

    // Operands from the table on page 50
    // x = (.110-1 10-1 1)
    // y = (.101-1 -1 110)
    int x_arr[] = {1, 1, 0, -1, 1, 0, -1, 1};
    int y_arr[] = {1, 0, 1, -1, -1, 1, 1, 0};

    cout << "========================================================================================\n";
    cout << setw(3) << "j" << " | "
         << setw(4) << "x_in" << " | "
         << setw(4) << "y_in" << " | "
         << setw(10) << "x[j+1]" << " | "
         << setw(10) << "y[j+1]" << " | "
         << setw(15) << "v[j]" << " | "
         << setw(4) << "p_out" << " | "
         << setw(15) << "w[j+1]" << "\n";
    cout << "========================================================================================\n";

    for (int i = 0; i < 8; ++i)
    {
        int j = i - 3; // Clock index (Starts at -3 for delta=3)

        TickResult res = mult.tick(x_arr[i], y_arr[i]);

        cout << setw(3) << j << " | "
             << setw(4) << x_arr[i] << " | "
             << setw(4) << y_arr[i] << " | "
             << setw(10) << res.x_j_str << " | "
             << setw(10) << res.y_j_str << " | "
             << setw(15) << res.v_j_str << " | "
             << setw(4) << res.p_out << " | "
             << setw(15) << res.w_j_str << "\n";
    }
    cout << "========================================================================================\n";

    return 0;
}
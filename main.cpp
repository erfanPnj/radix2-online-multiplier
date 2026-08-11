#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

// full adder
void FA(int a, int b, int c, int &cout, int &sum)
{
    sum = a ^ b ^ c;
    cout = (a & b) | (b & c) | (a & c);
}

// ca reg
template <size_t N>
class CAReg
{
public:
    int X[N + 1] = {0};
    int XM[N + 1] = {0};
    int step = 1;

    void append(int x_in, int max_steps)
    {
        if (step > max_steps)
            return;

        int next_X[N + 1] = {0};
        int next_XM[N + 1] = {0};

        for (int i = 1; i < step; ++i)
        {
            next_X[i] = X[i];
            next_XM[i] = XM[i];
        }

        // On-the-fly conversion routing
        if (x_in == 1)
        {
            next_X[step] = 1;
            next_XM[step] = 0;
            for (int i = 1; i < step; ++i)
            {
                next_XM[i] = X[i];
            }
        }
        else if (x_in == 0)
        {
            next_X[step] = 0;
            next_XM[step] = 1;
        }
        else if (x_in == -1)
        {
            next_X[step] = 1;
            next_XM[step] = 0;
            for (int i = 1; i < step; ++i)
            {
                next_X[i] = XM[i];
            }
        }

        for (int i = 1; i <= step; ++i)
        {
            X[i] = next_X[i];
            XM[i] = next_XM[i];
        }
        step++;
    }

    int get_bit(int i) const
    {
        return X[i];
    }
};

// String formatting helper for CA Register
template <size_t N>
std::string format_CA(const CAReg<N> &reg, int limit)
{
    std::string s = ".";
    int len = reg.step - 1;
    if (len > limit)
        len = limit;
    for (int i = 1; i <= len; ++i)
    {
        s += std::to_string(reg.get_bit(i));
    }
    return s;
}

// Software ripple-carry assimilation for printing terminal traces only
std::string format_assimilated(const int S[], const int C[], int SZ, int frac_bits)
{
    int sum_arr[200] = {0};
    int carry = 0;
    for (int i = SZ - 1; i >= 0; --i)
    {
        int s;
        FA(S[i], C[i], carry, carry, s);
        sum_arr[i] = s;
    }
    std::string res = std::to_string(sum_arr[0]) + std::to_string(sum_arr[1]) + ".";
    for (int i = 0; i < frac_bits; ++i)
    {
        res += std::to_string(sum_arr[i + 2]);
    }
    return res;
}

// Emulates an arithmetic right shift by 1 to print w[j+1] correctly from 2w[j+1]
std::string format_w_assimilated(const int S[], const int C[], int SZ, int frac_bits)
{
    int sum_arr[200] = {0};
    int carry = 0;
    for (int i = SZ - 1; i >= 0; --i)
    {
        int s;
        FA(S[i], C[i], carry, carry, s);
        sum_arr[i] = s;
    }

    int shifted[200] = {0};
    shifted[0] = sum_arr[0]; // Sign extension
    for (int i = 1; i < SZ; ++i)
    {
        shifted[i] = sum_arr[i - 1];
    }

    std::string res = std::to_string(shifted[0]) + std::to_string(shifted[1]) + ".";
    for (int i = 0; i < frac_bits; ++i)
    {
        res += std::to_string(shifted[i + 2]);
    }
    return res;
}

// Selector
template <size_t N>
void select_val(int dir, const CAReg<N> &reg, int A[], int SZ, int &c_out, int &lsb_idx_out)
{
    for (int i = 0; i < SZ; ++i)
        A[i] = 0;

    c_out = 0;
    lsb_idx_out = 0;

    if (dir != 0)
    {
        for (int i = 1; i < reg.step; ++i)
        {
            if (i + 4 < SZ)
                A[i + 4] = reg.get_bit(i);
        }
    }

    // 2's complement negation exactly at the dynamic boundaries
    if (dir == -1)
    {
        int lsb_idx = reg.step - 1 + 4;
        if (lsb_idx >= SZ)
            lsb_idx = SZ - 1;
        lsb_idx_out = lsb_idx;

        for (int i = 0; i <= lsb_idx; ++i)
        {
            A[i] = A[i] ^ 1;
        }

        // cx or cy signal
        c_out = 1;
    }
}

template <size_t N>
class OnlineMultiplier
{
    static constexpr int SZ = N + 8;
    int W_S[SZ] = {0};
    int W_C[SZ] = {0};
    CAReg<N> X_reg;
    CAReg<N> Y_reg;

private:
    // 4 to 2 adder
    void adder_4_to_2(const int A[], const int B[], int c_A, int lsb_A, int c_B, int lsb_B, int V_S[], int V_C[])
    {
        int c1_out[SZ] = {0};
        int s1[SZ] = {0};

        // first FA
        for (int i = 0; i < SZ; ++i)
        {
            FA(W_S[i], W_C[i], A[i], c1_out[i], s1[i]);
        }

        // secomd FA
        for (int i = 0; i < SZ; ++i)
        {
            int cin_layer2 = (i + 1 < SZ) ? c1_out[i + 1] : 0;
            if (i == lsb_A)
            {
                cin_layer2 |= c_A;
            }

            int c2;
            FA(s1[i], B[i], cin_layer2, c2, V_S[i]);
            if (i > 0)
            {
                V_C[i - 1] = c2;
            }
        }

        if (lsb_B < SZ)
        {
            V_C[lsb_B] |= c_B;
        }
    }

    // V block
    void execute_V_block(const int V_S[], const int V_C[], int v_est[])
    {
        int carry = 0;
        for (int i = 3; i >= 0; --i)
        {
            int sum;
            FA(V_S[i], V_C[i], carry, carry, sum);
            v_est[i] = sum;
        }
    }

    // SELM
    int execute_SELM(const int v_est[])
    {
        int v_m1 = v_est[0];
        int v_0 = v_est[1];
        int v_1 = v_est[2];

        int pp = (v_m1 ^ 1) & (v_0 | v_1);
        int pn = v_m1 & ((v_0 ^ 1) | (v_1 ^ 1));

        int p = 0;
        if (pp)
            p = 1;
        else if (pn)
            p = -1;

        return p;
    }

    // M block and left shifts
    void execute_M_block(int p, const int v_est[], const int V_S[], const int V_C[], int next_W_S[], int next_W_C[])
    {
        int abs_p = (p == 1 || p == -1) ? 1 : 0; // معادل منطقی pp | pn
        int v_0_star = v_est[1] ^ abs_p;

        next_W_S[0] = v_0_star;
        next_W_S[1] = v_est[2];
        next_W_S[2] = v_est[3];

        for (int i = 3; i < SZ; ++i)
        {
            next_W_S[i] = (i + 1 < SZ) ? V_S[i + 1] : 0;
            next_W_C[i] = (i + 1 < SZ) ? V_C[i + 1] : 0;
        }

        next_W_C[0] = 0;
        next_W_C[1] = 0;
        next_W_C[2] = 0;
    }

public:
    void run(const std::vector<int> &x_in, const std::vector<int> &y_in)
    {
        std::cout << " j | x_in | y_in |     x[j+1] |     y[j+1] |            v[j] | p_out |         w[j+1]\n";
        std::cout << "------------------------------------------------------------------------------------------\n";

        std::vector<int> p_out_seq;
        double final_result = 0.0;

        for (int j = -3; j < (int)N; ++j)
        {
            int k = j + 4;
            int xj4 = (k >= 1 && k <= x_in.size()) ? x_in[k - 1] : 0;
            int yj4 = (k >= 1 && k <= y_in.size()) ? y_in[k - 1] : 0;

            // x register & selection
            X_reg.append(xj4, N);
            int A[SZ], c_A = 0, lsb_A = 0;
            select_val(xj4, Y_reg, A, SZ, c_A, lsb_A);

            // y register & selection
            Y_reg.append(yj4, N);
            int B[SZ], c_B = 0, lsb_B = 0;
            select_val(yj4, X_reg, B, SZ, c_B, lsb_B);

            // [4:2] adder
            int V_S[SZ] = {0};
            int V_C[SZ] = {0};
            adder_4_to_2(A, B, c_A, lsb_A, c_B, lsb_B, V_S, V_C);

            // V block
            int v_est[4] = {0};
            execute_V_block(V_S, V_C, v_est);

            // selm
            int p = execute_SELM(v_est);

            // M block
            int next_W_S[SZ] = {0};
            int next_W_C[SZ] = {0};
            execute_M_block(p, v_est, V_S, V_C, next_W_S, next_W_C);

            // ---------------------------------------
            // Trace Printing Formatting
            // ---------------------------------------
            int frac_bits = j + 7;
            if (j > (int)N - 4)
            {
                frac_bits = N + 3 - (j - (N - 4));
            }

            std::string sx = (k >= 1 && k <= (int)N) ? std::to_string(xj4) : "0";
            std::string sy = (k >= 1 && k <= (int)N) ? std::to_string(yj4) : "0";
            std::string sp = std::to_string(p);

            std::string s_x_reg = format_CA(X_reg, N);
            std::string s_y_reg = format_CA(Y_reg, N);
            std::string s_v = format_assimilated(V_S, V_C, SZ, frac_bits);
            std::string s_w = format_w_assimilated(next_W_S, next_W_C, SZ, frac_bits);

            for (int i = 0; i < SZ; ++i)
            {
                W_S[i] = next_W_S[i];
                W_C[i] = next_W_C[i];
            }

            std::cout << std::setw(2) << j << " | "
                      << std::setw(4) << sx << " | "
                      << std::setw(4) << sy << " | "
                      << std::setw(10) << s_x_reg << " | "
                      << std::setw(10) << s_y_reg << " | "
                      << std::setw(15) << s_v << " | "
                      << std::setw(5) << sp << " | "
                      << std::setw(15) << s_w << "\n";

            // Accumulate output digit logic
            if (j >= 0)
            {
                p_out_seq.push_back(p);
                final_result += p * std::pow(2.0, -(j + 1));
            }
        }

        // Output Result Verification block
        double expected_x = 0;
        double expected_y = 0;
        for (size_t i = 0; i < x_in.size(); ++i)
            expected_x += x_in[i] * std::pow(2.0, -(int)(i + 1));
        for (size_t i = 0; i < y_in.size(); ++i)
            expected_y += y_in[i] * std::pow(2.0, -(int)(i + 1));

        double exact_product = expected_x * expected_y;
        double truncation_error = std::abs(exact_product - final_result);

        std::cout << "\n================================ FINAL RESULTS ===============================\n";
        std::cout << "Hardware P_out Sequence : 0.";
        for (int p_val : p_out_seq)
        {
            if (p_val == -1)
                std::cout << "[-1]";
            else
                std::cout << p_val;
        }

        std::cout << "\n\n";
        std::cout << std::fixed << std::setprecision(8);
        std::cout << "Computed SD Result (Dec): " << final_result << "\n";
        std::cout << "Expected x_in * y_in    : " << expected_x << " * " << expected_y << " = " << exact_product << "\n";
        std::cout << "Truncation Error        : " << truncation_error << "\n";
        std::cout << "Target Error Bound      : " << std::pow(2.0, -(int)N) << " (2^" << -(int)N << ")" << "\n";
        std::cout << "==============================================================================\n";
    }
};

int main()
{
    std::vector<int> x = {1, 1, 0, -1, 1, 0, -1, 1};
    std::vector<int> y = {1, 0, 1, -1, -1, 1, 1, 0};

    OnlineMultiplier<16> mult;
    mult.run(x, y);

    return 0;
}
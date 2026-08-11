#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <algorithm>

using namespace std;

// Standard gate-level Full Adder
void FA(int a, int b, int c, int &cout, int &sum)
{
    sum = a ^ b ^ c;
    cout = (a & b) | (b & c) | (a & c);
}

// Convert-and-Append (CA) Register
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
            return; // Freeze after maximum digits are reached

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
            // X retains X, XM retains XM
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

// String formatting helpers for trace output
template <size_t N>
string format_CA(const CAReg<N> &reg, int limit)
{
    string s = ".";
    int len = reg.step - 1;
    if (len > limit)
        len = limit;
    for (int i = 1; i <= len; ++i)
    {
        s += to_string(reg.get_bit(i));
    }
    return s;
}

string format_assimilated(const int S[], const int C[], int SZ, int frac_bits)
{
    int sum_arr[200] = {0};
    int carry = 0;
    // Ripple carry is used here strictly for printing the terminal trace
    for (int i = SZ - 1; i >= 0; --i)
    {
        int s;
        FA(S[i], C[i], carry, carry, s);
        sum_arr[i] = s;
    }
    string res = to_string(sum_arr[0]) + to_string(sum_arr[1]) + ".";
    for (int i = 0; i < frac_bits; ++i)
    {
        res += to_string(sum_arr[i + 2]);
    }
    return res;
}

// Selector Block
template <size_t N>
void select_val(int dir, const CAReg<N> &reg, int A[], int &c, int SZ)
{
    for (int i = 0; i < SZ; ++i)
        A[i] = 0;

    if (dir != 0)
    {
        for (int i = 1; i < reg.step; ++i)
        {
            if (i + 4 < SZ)
                A[i + 4] = reg.get_bit(i);
        }
    }

    // 2's complement negation setup if negative digit
    if (dir == -1)
    {
        for (int i = 0; i < SZ; ++i)
        {
            A[i] = 1 - A[i];
        }
        c = 1; // Inject carry into the lowest FA to complete 2's complement
    }
    else
    {
        c = 0;
    }
}

template <size_t N>
class OnlineMultiplier
{
    static constexpr int SZ = N + 8; // Bit width mapping
    int W_S[SZ] = {0};
    int W_C[SZ] = {0};
    CAReg<N> X_reg;
    CAReg<N> Y_reg;

public:
    void run(const vector<int> &x_in, const vector<int> &y_in)
    {
        cout << " j | x_in | y_in |     x[j+1] |     y[j+1] |            v[j] | p_out |          w[j+1]\n";
        cout << "------------------------------------------------------------------------------------------\n";

        for (int j = -3; j < (int)N; ++j)
        {
            int k = j + 4;
            int xj4 = (k >= 1 && k <= (int)N) ? x_in[k - 1] : 0;
            int yj4 = (k >= 1 && k <= (int)N) ? y_in[k - 1] : 0;

            // Notice the asymmetric registry: X_reg updates before selection to model X[j+1]
            X_reg.append(xj4, N);

            int A[SZ], c_A;
            select_val(xj4, Y_reg, A, c_A, SZ);

            // Y_reg updates after X selection to model Y[j] for the top branch
            Y_reg.append(yj4, N);

            int B[SZ], c_B;
            select_val(yj4, X_reg, B, c_B, SZ);

            // Shift W by 1 for 2W (wired left shift)
            int W2_S[SZ] = {0}, W2_C[SZ] = {0};
            for (int i = 0; i < SZ - 1; ++i)
            {
                W2_S[i] = W_S[i + 1];
                W2_C[i] = W_C[i + 1];
            }
            // Inject negated carries into vacated LSB positions
            W2_S[SZ - 1] = c_A;
            W2_C[SZ - 1] = c_B;

            // [4:2] Compressor Array - Layer 1
            int cout_arr[SZ] = {0};
            int s1_arr[SZ] = {0};
            for (int i = 0; i < SZ; ++i)
            {
                FA(W2_S[i], W2_C[i], A[i], cout_arr[i], s1_arr[i]);
            }

            // [4:2] Compressor Array - Layer 2 (generating v_s and v_c)
            int V_S[SZ] = {0};
            int V_C[SZ] = {0};
            for (int i = 0; i < SZ; ++i)
            {
                int cin = (i == SZ - 1) ? 0 : cout_arr[i + 1];
                int c2, s2;
                FA(s1_arr[i], B[i], cin, c2, s2);
                V_S[i] = s2;
                if (i > 0)
                {
                    V_C[i - 1] = c2;
                }
            }
            V_C[SZ - 1] = 0;

            // V Block Assimilation (Top 6 bits: indices 0 to 5)
            int carry = 0;
            int assimilated[6] = {0};
            for (int i = 5; i >= 0; --i)
            {
                int sum;
                FA(V_S[i], V_C[i], carry, carry, sum);
                assimilated[i] = sum;
            }

            // SELM Selection Logic
            int val = 0;
            for (int i = 0; i <= 5; ++i)
            {
                val = (val << 1) | assimilated[i];
            }
            if (assimilated[0] == 1)
                val |= ~0x3F; // Sign extend the 6-bit estimate

            int p = 0;
            if (val >= 8)
                p = 1;
            else if (val <= -8)
                p = -1;
            else
                p = 0;

            // M Block: Subtraction logic represented purely by logic vectors
            int P_vec[6] = {0};
            if (p == 1)
            {
                // Equivalent to adding 2's complement of 1 (-16 in shifted fixed point)
                P_vec[0] = 1;
                P_vec[1] = 1;
                P_vec[2] = 0;
                P_vec[3] = 0;
                P_vec[4] = 0;
                P_vec[5] = 0;
            }
            else if (p == -1)
            {
                // Equivalent to adding 1 (16 in shifted fixed point)
                P_vec[0] = 0;
                P_vec[1] = 1;
                P_vec[2] = 0;
                P_vec[3] = 0;
                P_vec[4] = 0;
                P_vec[5] = 0;
            }

            int m_carry = 0;
            for (int i = 5; i >= 0; --i)
            {
                int s;
                FA(assimilated[i], P_vec[i], m_carry, m_carry, s);
                W_S[i] = s;
                W_C[i] = 0; // Top WC bits zeroed out post-subtraction
            }

            // Lower unassimilated bits pass through
            for (int i = 6; i < SZ; ++i)
            {
                W_S[i] = V_S[i];
                W_C[i] = V_C[i];
            }

            // Dynamically scope fractional print lengths to align perfectly with the textbook
            int frac_bits = j + 7;
            if (j > (int)N - 4)
            {
                frac_bits = N + 3 - (j - (N - 4));
            }

            string sx = (k >= 1 && k <= (int)N) ? to_string(xj4) : "0";
            string sy = (k >= 1 && k <= (int)N) ? to_string(yj4) : "0";
            string sp = to_string(p);

            string s_x_reg = format_CA(X_reg, N);
            string s_y_reg = format_CA(Y_reg, N);
            string s_v = format_assimilated(V_S, V_C, SZ, frac_bits);
            string s_w = format_assimilated(W_S, W_C, SZ, frac_bits);

            cout << setw(2) << j << " | "
                 << setw(4) << sx << " | "
                 << setw(4) << sy << " | "
                 << setw(10) << s_x_reg << " | "
                 << setw(10) << s_y_reg << " | "
                 << setw(15) << s_v << " | "
                 << setw(5) << sp << " | "
                 << setw(15) << s_w << "\n";
        }
    }
};

int main()
{
    // Page 50 operand initialization
    vector<int> x = {1, 1, 0, -1, 1, 0, -1, 1};
    vector<int> y = {1, 0, 1, -1, -1, 1, 1, 0};

    OnlineMultiplier<8> mult;
    mult.run(x, y);

    return 0;
}
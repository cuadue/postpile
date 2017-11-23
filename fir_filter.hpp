/* (C) 2016 Wes Waugh
 * GPLv3 License: respect Stallman because he is right.
 */

/* TODO This is wildly inefficient thanks in advance
 */
#pragma once
#include <vector>
#include <list>
#include <cmath>

template<typename T>
class fir_filter {
    std::vector<float> coeff;
    std::list<T> state;

public:
    fir_filter(const std::vector<float> _coeff, const T &initial_value)
    : coeff(_coeff)
    , state(coeff.size())
    {
        float sum = 0;
        for (float c : coeff) sum += c;
        float gain = 1/sum;
        for (float &c : coeff) c *= gain;
        for (unsigned i = 0; i < coeff.size(); i++)
            next(initial_value);
    }

    T next(const T &x) {
        int i = 0;
        for (T &v : state) {
            v += coeff[i++] * x;
        }
        state.push_back(T());
        T ret = state.front();
        state.pop_front();
        return ret;
    }

    T get() const { return state.front(); }
};

template<typename T>
class filtered_value {
    T big, lil, final_value;
    fir_filter<T> filter;
public:
    filtered_value(const std::vector<float> &filter_coeffs,
                   const T &_big=INFINITY, const T &_lil=-INFINITY)
    : big(_big)
    , lil(_lil)
    , final_value(isinf(big) || isinf(lil) ? 0 : (big + lil) / 2.0)
    , filter(filter_coeffs, final_value)
    {
    }

    void add(const T &x) {
        final_value += x;
        if (std::isfinite(big))
            final_value = std::min(big, final_value);
        if (std::isfinite(lil))
            final_value = std::max(lil, final_value);

        filter.next(final_value);
    }

    T get() const { return filter.get(); }
};

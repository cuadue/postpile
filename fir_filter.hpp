#pragma once
#include <vector>
#include <list>

template<typename T>
class fir_filter {
    std::vector<float> coeff;
    std::list<T> state;

public:
    fir_filter(const std::vector<float> _coeff)
    : coeff(_coeff)
    , state(coeff.size())
    {
        float sum = 0;
        for (float c : coeff) sum += c;
        float gain = 1/sum;
        for (float &c : coeff) c *= gain;
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
};

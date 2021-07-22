// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once
#include <math.h>
#include <stdio.h>

#include <vector>
template <typename ValueType>
class Histogram
{
  public:
    Histogram()
    {
        num_entries_ = 0;
        num_low_outliers_ = 0;
        num_high_outliers_ = 0;
        low_percentile_ = 0;
        high_percentile_ = 0;
        SetWindowSize(10000);
        SetCumulativeDensityThresholds(0.01f, 0.99f);
    }

    void AddSample(ValueType s)
    {
        int N = static_cast<int>(samples_.size());
        samples_[num_entries_ % N] = s;
        num_entries_++;
    }

    void SetBucketCount(int bc)
    {
        buckets_.resize(bc);
    }

    void SetWindowSize(int s)
    {
        samples_.resize(s);
    }

    void SetCumulativeDensityThresholds(float low_cd, float high_cd)
    {
        low_cum_density_ = low_cd;
        high_cum_density_ = high_cd;
    }

    void ComputeHistogram()
    {
        const int NS = static_cast<int>(samples_.size());
        int N = NS;
        if (num_entries_ < N)
        {
            N = num_entries_;
        }
        if (N <= 0)
        {
            return;
        }
        std::vector<ValueType> sorted;
        if (N == NS)
            sorted = samples_;
        else
            sorted.insert(sorted.end(), samples_.begin(), samples_.begin() + N);
        sort(sorted.begin(), sorted.end());
        int idx_low = static_cast<int>(N * low_cum_density_);
        int idx_high = static_cast<int>(N * high_cum_density_);
        if (idx_high - idx_low + 1 < 1)
        {
            return; // No entries can be shown, quit
        }
        max_bucket_height_ = 0;
        ValueType value_low = sorted[idx_low];
        ValueType value_high = sorted[idx_high];
        low_percentile_ = value_low;
        high_percentile_ = value_high;
        const int NB = static_cast<int>(buckets_.size()); // Number of Bins
        float bucket_width = (value_high - value_low) / NB;
        // If all values are the same, manually extend the range to
        // (value-1, value+1)
        const float EPS = 1e-4;
        if (fabs(value_high - value_low) <= EPS)
        {
            value_low = value_low - 1;
            value_high = value_high + 1;
            bucket_width = (value_high - value_low) / NB;
        }
        else
        {}
        buckets_.assign(NB, 0);
        num_low_outliers_ = 0;
        num_high_outliers_ = 0;
        for (int i = idx_low; i <= idx_high; i++)
        {
            ValueType v = sorted[i];
            ValueType dist = (v - value_low);
            int bucket_idx = dist / bucket_width;
            if (bucket_idx < 0)
            {
                num_low_outliers_++;
            }
            else if (bucket_idx >= NB)
            {
                num_high_outliers_++;
            }
            else
            {
                buckets_[bucket_idx]++;
                max_bucket_height_ =
                    std::max(max_bucket_height_, buckets_[bucket_idx]);
            }
        }
    }

    int BucketHeight(int idx)
    {
        if (idx < 0 || idx >= static_cast<int>(buckets_.size()))
            return 0;
        return buckets_[idx];
    }
    
    void Assign(Histogram<ValueType>* out)
    {
        out->num_entries_ = num_entries_;
        out->samples_ = samples_;
        out->num_low_outliers_ = num_low_outliers_;
        out->num_high_outliers_ = num_high_outliers_;
        out->buckets_ = buckets_;
        out->low_cum_density_ = low_cum_density_;
        out->high_cum_density_ = high_cum_density_;
        out->low_percentile_ = low_percentile_;
        out->high_percentile_ = high_percentile_;
        out->max_bucket_height_ = max_bucket_height_;
    }

    int MaxBucketHeight()
    {
        return max_bucket_height_;
    }

    ValueType LowPercentile()
    {
        return low_percentile_;
    }

    ValueType HighPercentile()
    {
        return high_percentile_;
    }

    float LowCumDensity()
    {
        return low_cum_density_;
    }

    float HighCumDensity()
    {
        return high_cum_density_;
    }

    bool Empty()
    {
        return num_entries_ == 0;
    }
    
    int num_entries_;
    std::vector<ValueType> samples_;
    int num_low_outliers_, num_high_outliers_;
    std::vector<int> buckets_;
    float low_cum_density_, high_cum_density_;
    // "1% percentile" means "1% of the samples are below this value"
    ValueType low_percentile_, high_percentile_;
    int max_bucket_height_;
};
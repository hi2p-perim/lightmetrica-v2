/*
    Lightmetrica - A modern, research-oriented renderer

    Copyright (c) 2015 Hisanari Otsu

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

#include <pch.h>
#include <lightmetrica/detail/photonmap.h>
#include <lightmetrica/bound.h>

LM_NAMESPACE_BEGIN

struct PhotonKdTreeNode
{
    bool isleaf;
    Bound bound;
    
    union
    {
        struct
        {
            int begin;
            int end;
        } leaf;

        struct
        {
            int child1;
            int child2;
        } internal;
    };
};

class PhotonMap_KdTree : public PhotonMap
{
public:

    LM_IMPL_CLASS(PhotonMap_KdTree, PhotonMap);

public:

    virtual auto Build(const std::vector<Photon>& photons) -> void
    {
        // Build function
        int processedPhotons = 0;
        const std::function<int(int, int)> Build_ = [&](int begin, int end) -> int
        {
            int idx = (int)(nodes_.size());
            nodes_.emplace_back(new PhotonKdTreeNode);
            auto* node = nodes_[idx].get();

            // Current bound
            node->bound = Bound();
            for (int i = begin; i < end; i++)
            {
                node->bound = Math::Union(node->bound, photons_[indices_[i]].p);
            }

            // Create leaf node
            const int LeafNumNodes = 10;
            if (end - begin < LeafNumNodes)
            {
                node->isleaf = true;
                node->leaf.begin = begin;
                node->leaf.end = end;

                // Progress update
                processedPhotons += end - begin;
                const double progress = (double)(processedPhotons) / photons.size() * 100.0;
                LM_LOG_INPLACE(boost::str(boost::format("Progress: %.1f%%") % progress));
                
                return idx;
            }

            // Select longest axis as split axis
            const int axis = node->bound.LongestAxis();

            // Select split position
            const Float split = node->bound.Centroid()[axis];

            // Partition into two sets according to split position
            const auto it = std::partition(indices_.begin() + begin, indices_.begin() + end, [&](int i) -> bool
            {
                return photons_[i].p[axis] < split;
            });

            // Create intermediate node
            const int mid = (int)(std::distance(indices_.begin(), it));
            node->isleaf = false;
            node->internal.child1 = Build_(begin, mid);
            node->internal.child2 = Build_(mid, end);

            return idx;
        };

        photons_ = photons;
        nodes_.clear();
        indices_.assign(photons.size(), 0);
        std::iota(indices_.begin(), indices_.end(), 0);
        Build_(0, (int)(photons.size()));

        LM_LOG_INFO("Progress: 100.0%");
    }

    virtual auto CollectPhotons(const Vec3& p, int n, Float maxDist2, std::vector<Photon>& collected) const -> Float
    {
        collected.clear();

        const auto comp = [&](const Photon& p1, const Photon& p2)
        {
            return Math::Length2(p1.p - p) < Math::Length2(p2.p - p);
        };

        const std::function<void(int)> Collect = [&](int idx) -> void
        {
            const auto* node = nodes_.at(idx).get();
            
            if (node->isleaf)
            {
                for (int i = node->leaf.begin; i < node->leaf.end; i++)
                {
                    const auto& photon = photons_[indices_[i]];
                    const auto dist2 = Math::Length2(photon.p - p);
                    if (dist2 < maxDist2)
                    {
                        if ((int)(collected.size()) < n)
                        {
                            collected.push_back(photon);
                            if ((int)(collected.size()) == n)
                            {
                                // Create heap
                                std::make_heap(collected.begin(), collected.end(), comp);
                                maxDist2 = Math::Length2(collected.front().p - p);
                            }
                        }
                        else
                        {
                            // Update heap
                            std::pop_heap(collected.begin(), collected.end(), comp);
                            collected.back() = photon;
                            std::push_heap(collected.begin(), collected.end(), comp);
                            maxDist2 = Math::Length2(collected.front().p - p);
                        }
                    }
                }
                return;
            }

            const int axis = node->bound.LongestAxis();
            const Float split = node->bound.Centroid()[axis];
            const auto dist2 = (p[axis] - split) * (p[axis] - split);
            if (p[axis] < split)
            {
                Collect(node->internal.child1);
                if (dist2 < maxDist2)
                {
                    Collect(node->internal.child2);
                }
            }
            else
            {
                Collect(node->internal.child2);
                if (dist2 < maxDist2)
                {
                    Collect(node->internal.child1);
                }
            }
        };

        Collect(0);

        return maxDist2;
    }

private:

    std::vector<std::unique_ptr<PhotonKdTreeNode>> nodes_;
    std::vector<int> indices_;
    std::vector<Photon> photons_;

};

LM_COMPONENT_REGISTER_IMPL(PhotonMap_KdTree, "photonmap::kdtree");

LM_NAMESPACE_END

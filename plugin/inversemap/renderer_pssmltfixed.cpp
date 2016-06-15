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

#include "inversemaputils.h"

LM_NAMESPACE_BEGIN

///! Primary sample space metropolis light transport (fixed path length)
class Renderer_PSSMLTFixed final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_PSSMLTFixed, Renderer);

public:

    int numVertices_;
    long long numMutations_;

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        if (!prop->ChildAs<int>("num_vertices", numVertices_)) return false;
        if (!prop->ChildAs<long long>("num_mutations", numMutations_)) return false;
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene, Random* initRng, Film* film) -> void
    {
        //region Thread-specific context
        struct Context
        {
            Random rng;
            Film::UniquePtr film{nullptr, nullptr};
        };
        std::vector<Context> contexts(Parallel::GetNumThreads());
        for (auto& ctx : contexts)
        {
            ctx.rng.SetSeed(initRng->NextUInt());
            ctx.film = ComponentFactory::Clone<Film>(film);

            //region Initial state
            while (true)
            {
                // Generate initial sample with positive contribution with path tracing
                // Ignore start-up bias here
                std::vector<double> ps;
                for (int i = 0; i < Params.NumVertices; i++)
                {
                    ps.push_back(initRng.Next());
                }

                const auto p = MapPS2Path(scene, ps);
                if ((int)(p.vertices.size()) != Params.NumVertices || p.EvaluateF() == 0.0)
                {
                    continue;
                }

                ctx.currPS = ps;
                break;
            }
            //endregion
        }
        //endregion

        // --------------------------------------------------------------------------------

        Parallel::For(numMutations_, [&](long long index, int threadid, bool init)
        {
            auto& ctx = contexts[threadid];

            //region Small step mutation in primary sample space
            std::vector<double> propPS;
            {
                //region Mutate
                const auto LargeStep = [this](const std::vector<double>& currPS, Random& rng) -> std::vector <double>
                {
                    assert(currPS.size() == Params.NumVertices);
                    std::vector<double> propPS;
                    for (int i = 0; i < Params.NumVertices; i++)
                    {
                        propPS.push_back(rng.Next());
                    }
                    return propPS;
                };

                const auto SmallStep = [this](const std::vector<double>& ps, Random& rng) -> std::vector<double>
                {
                    const auto Perturb = [](Random& rng, const double u, const double s1, const double s2)
                    {
                        double result;
                        double r = rng.Next();
                        if (r < 0.5)
                        {
                            r = r * 2.0;
                            result = u + s2 * glm::exp(-glm::log(s2 / s1) * r);
                            if (result > 1.0) result -= 1.0;
                        }
                        else
                        {
                            r = (r - 0.5) * 2.0;
                            result = u - s2 * glm::exp(-glm::log(s2 / s1) * r);
                            if (result < 0.0) result += 1.0;
                        }
                        return result;
                    };

                    std::vector<double> propPS;
                    for (const double u : ps)
                    {
                        propPS.push_back(Perturb(rng, u, 1.0 / 1024.0, 1.0 / 64.0));
                    }

                    return propPS;
                };

                //propPS = SmallStep(ctx.currPS, ctx.rng);
                propPS = LargeStep(ctx.currPS, ctx.rng);
                //endregion

                // --------------------------------------------------------------------------------

                //region MH update
                // Function to compute path contribution
                const auto PathContrb = [](const Path& path)
                {
                    const double F = path.EvaluateF();
                    assert(F >= 0);
                    assert(!glm::isnan(F));
                    double C = 0;
                    if (F > 0)
                    {
                        const double P = path.EvaluatePDF((int)(path.vertices.size()));
                        assert(P > 0);
                        C = F / P;
                    }
                    return C;
                };

                // Map primary samples to paths
                const auto currP = MapPS2Path(scene, ctx.currPS);
                const auto propP = MapPS2Path(scene, propPS);

                // Immediately rejects if the dimension changes
                if (currP.vertices.size() == propP.vertices.size())
                {
                    // Evaluate contributions
                    const double currC = PathContrb(currP);
                    const double propC = PathContrb(propP);

                    // Acceptance ratio
                    const double A = currC == 0 ? 1 : glm::min(1.0, propC / currC);

                    // Accept or reject?
                    if (ctx.rng.Next() < A)
                    {
                        ctx.currPS.swap(propPS);
                    }
                }
                //endregion
            }
            //endregion

            // --------------------------------------------------------------------------------

            //region Accumulate contribution
            {
                auto currP = MapPS2Path(scene, ctx.currPS);
                const double currF = currP.EvaluateF();
                if (currF > 0)
                {
                    // We ignored norm factor here, because we compare result offset with same normalization factors.
                    ctx.film->Splat(currP.RasterPosition(), Vec3(1_f));
                }
            }
            //endregion
        };
    };

};

LM_COMPONENT_REGISTER_IMPL(Renderer_PSSMLTFixed, "renderer::vcm");

LM_NAMESPACE_END

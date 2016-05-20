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

#include <lightmetrica/logger.h>
#include <lightmetrica/exception.h>
#include <lightmetrica/property.h>
#include <lightmetrica/scene.h>
#include <lightmetrica/renderer.h>
#include <lightmetrica/assets.h>
#include <lightmetrica/accel.h>
#include <lightmetrica/film.h>
#include <lightmetrica/primitive.h>
#include <lightmetrica/sensor.h>
#include <lightmetrica/detail/propertyutils.h>
#include <lightmetrica/detail/stringtemplate.h>
#include <lightmetrica/detail/version.h>
#include <lightmetrica/detail/parallel.h>
#include <lightmetrica/fp.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <chrono>
#include <iomanip>
#include <ctime>

#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <boost/optional.hpp>
#include <boost/filesystem.hpp>

using namespace lightmetrica_v2;

// --------------------------------------------------------------------------------

#pragma region Helper functions

namespace
{
    auto MultiLineLiteral(const std::string& text) -> std::string
    {
        std::string converted;
        const std::regex re(R"x(^ *\| ?(.*)$)x");
        std::stringstream ss(text);
        std::string line;
        while (std::getline(ss, line))
        {
            std::smatch m;
            if (std::regex_match(line, m, re))
            {
                converted += std::string(m[1]) + "\n";
            }
        }
        return converted;
    };
}

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Program options

enum class SubcommandType
{
    Help,
    Render,
    //Verify,
};

struct ProgramOption
{
    
    SubcommandType Type;
    struct
    {
        bool Help = false;
        std::string HelpDetail;
        std::string SceneFile;
        std::string OutputPath;
        bool Verbose;
        std::unordered_map<std::string, std::string> TemplateDict;
    } Render;

public:

    ProgramOption() {}

public:

    auto Parse(int argc, char** argv) -> bool
    {
        namespace po = boost::program_options;

        po::options_description globalOpt("Global options");
        globalOpt.add_options()
            ("subcommand", po::value<std::string>(), "Subcommand to execute")
            ("subargs", po::value<std::vector<std::string>>(), "Arguments for command");

        po::positional_options_description pos;
        pos.add("subcommand", 1).
            add("subargs", -1);

        po::variables_map vm;
        try
        {
            po::parsed_options parsed = po::command_line_parser(argc, argv)
                .options(globalOpt)
                .positional(pos)
                .allow_unregistered()
                .run();

            po::store(parsed, vm);

            // --------------------------------------------------------------------------------

            if (argc == 1)
            {
                Type = SubcommandType::Help;
                return true;
            }

            // --------------------------------------------------------------------------------

            #pragma region Process subcommands

            if (vm.count("subcommand") == 0)
            {
                LM_LOG_ERROR_SIMPLE("Error on program options : Invalid subcommand");
                return false;
            }

            const auto subcmd = vm["subcommand"].as<std::string>();

            {
                #pragma region Process help subcommand

                if (subcmd == "help")
                {
                    Type = SubcommandType::Help;
                    return true;
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Process render subcommand

                if (subcmd == "render")
                {
                    Type = SubcommandType::Render;

                    po::options_description renderOpt("Options");
                    renderOpt.add_options()
                        ("help", "Display help message (this message)")
                        ("scene,s", po::value<std::string>()->required(), "Scene configuration file")
                        ("output,o", po::value<std::string>()->default_value("result"), "Output image")
                        ("num-threads,j", po::value<int>(), "Number of threads")
                        ("verbose,v", po::bool_switch()->default_value(false), "Adds detailed information on the output")
                        ("template,t", po::value<std::vector<std::string>>()->multitoken()->zero_tokens()->composing(), "String templates");

                    auto opts = po::collect_unrecognized(parsed.options, po::include_positional);
                    opts.erase(opts.begin());

                    po::store(po::command_line_parser(opts).options(renderOpt).run(), vm);
                    if (vm.count("help") || opts.empty())
                    {
                        std::stringstream ss;
                        ss << renderOpt;
                        Render.Help = true;
                        Render.HelpDetail = ss.str();
                        return true;
                    }

                    po::notify(vm);

                    Render.SceneFile  = vm["scene"].as<std::string>();
                    Render.OutputPath = vm["output"].as<std::string>();
                    Render.Verbose    = vm["verbose"].as<bool>();

                    if (vm.count("template"))
                    {
                        for (const auto& str : vm["template"].as<std::vector<std::string>>())
                        {
                            // Parse {key} = {value}
                            std::regex re(R"x((\w+) *= *(.+))x");
                            std::smatch match;
                            if (!std::regex_match(str, match, re))
                            {
                                continue;
                            }

                            Render.TemplateDict[match[1]] = match[2];
                        }
                    }

                    if (vm.count("num-threads"))
                    {
                        Parallel::SetNumThreads(vm["num-threads"].as<int>());
                    }

                    return true;
                }

                #pragma endregion
            
                // --------------------------------------------------------------------------------

                #if 0
                #pragma region Process verify subcommand

                if (subcmd == "verify")
                {
                    Type = SubcommandType::Verify;
                    return true;
                }

                #pragma endregion
                #endif
            }

            throw po::invalid_option_value(subcmd);

            #pragma endregion
        }
        catch (po::required_option& e)
        {
            LM_LOG_ERROR_SIMPLE("Error on program options : " + std::string(e.what()));
        }
        catch (po::error& e)
        {
            LM_LOG_ERROR_SIMPLE("Error on program options : " + std::string(e.what()));
        }

        return false;
    }

};

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Main application

/*
    Main application class.
    TODO
      - Add verification of scene file
*/
class Application
{
public:

    bool Run(int argc, char** argv)
    {
        ProgramOption opt;
        if (!opt.Parse(argc, argv))
        {
            ProcessCommand_Help(opt);
            return false;
        }

        switch (opt.Type)
        {
            case SubcommandType::Help:   { return ProcessCommand_Help(opt);   }
            case SubcommandType::Render: { return ProcessCommand_Render(opt); }
        }

        return false;
    }

private:

    auto ProcessCommand_Help(const ProgramOption& opt) -> bool
    {
        LM_LOG_INFO_SIMPLE(MultiLineLiteral(R"x(
        |
        | Usage: lightmetrica [subcommand] [options]
        | 
        | Welcome to Lightmetrica!
        |
        | Lightmetrica: A modern, research-oriented renderer
        | Documentation: http://lightmetrica.org/doc
        |
        | Subcommands:
        | 
        | - lightmetrica help
        |   Print global help message (this message).
        |
        | - lightmetrica render
        |   Render the image.
        |   `lightmetrica render --help` for more detailed help.
        |
        )x"));
        return true;
    }

    auto ProcessCommand_Render(const ProgramOption& opt) -> bool
    {
        #pragma region Configure logger

        Logger::SetVerboseLevel(opt.Render.Verbose ? 2 : 0);

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Handle help message

        if (opt.Render.Help)
        {
            LM_LOG_INFO_SIMPLE("");
            LM_LOG_INFO_SIMPLE("Usage: lightmetrica render [options]");
            LM_LOG_INFO_SIMPLE("");
            LM_LOG_INFO_SIMPLE(opt.Render.HelpDetail);
            return true;
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Print initial message

        {
            #pragma region Header

            if (opt.Render.Verbose)
            {
                LM_LOG_INFO_SIMPLE("| TYPE  TIME  | FILENAME  | LINE  | TID |");
            }

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Current time

            std::string currentTime;
            {
                std::stringstream ss;
                auto now = std::chrono::system_clock::now();
                auto time = std::chrono::system_clock::to_time_t(now);
                #if LM_PLATFORM_WINDOWS
                struct tm timeinfo;
                localtime_s(&timeinfo, &time);
                ss << std::put_time(&timeinfo, "%Y.%m.%d.%H.%M.%S");
                #elif LM_PLATFORM_LINUX || LM_PLATFORM_APPLE
                char timeStr[256];
                std::strftime(timeStr, sizeof(timeStr), "%Y.%m.%d.%H.%M.%S", std::localtime(&time));
                ss << timeStr;
                #endif
                currentTime = ss.str();
            };

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Application flags

            std::string flags;
            {
                flags += LM_SINGLE_PRECISION ? "single_precision " : "";
                flags += LM_DOUBLE_PRECISION ? "double_precision " : "";
                flags += LM_SSE ? "sse " : "";
                flags += LM_AVX ? "avx " : "";
            }

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Print message

            const auto message = MultiLineLiteral(R"x(
            |
            | Lightmetrica
            |
            | A modern, research-oriented renderer
            | Version {{version}} ({{codename}})
            |
            | Copyright (c) 2015 Hisanari Otsu
            | The software is distributed under the MIT license.
            | For detail see the LICENSE file along with the software.
            |
            | BUILD DATE   | {{date}}
            | PLATFORM     | {{platform}} {{arch}}
            | FLAGS        | {{flags}}
            | CURRENT TIME | {{time}}
            |
            )x");

            std::unordered_map<std::string, std::string> dict;
            dict["version"]  = Version::Formatted();
            dict["codename"] = Version::Codename();
            dict["date"]     = Version::BuildDate();
            dict["platform"] = Version::Platform();
            dict["arch"]     = Version::Archtecture();
            dict["flags"]    = flags;
            dict["time"]     = currentTime;

            LM_LOG_INFO(StringTemplate::Expand(message, dict));

            #pragma endregion
        }

        #pragma endregion
        
        // --------------------------------------------------------------------------------

        #pragma region Load plugins

        // TODO: Make configurable plugin directory
        {
            LM_LOG_INFO("Loading plugins");
            LM_LOG_INDENTER();
            ComponentFactory::LoadPlugins("plugin");
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Load configuration files

        // Scene configuration
        const auto sceneConf = ComponentFactory::Create<PropertyTree>();
        {
            LM_LOG_INFO("Loading scene file");
            LM_LOG_INDENTER();
            LM_LOG_INFO("Loading '" + opt.Render.SceneFile + "'");

            // Load from file
            std::ifstream t(opt.Render.SceneFile);
            if (!t.is_open())
            {
                LM_LOG_ERROR("Failed to open: " + opt.Render.SceneFile);
                return false;
            }

            std::stringstream ss;
            ss << t.rdbuf();

            // Expand template & load scene file
            const auto expanded = StringTemplate::Expand(ss.str(), opt.Render.TemplateDict);
            if (!sceneConf->LoadFromStringWithFilename(expanded, opt.Render.SceneFile))
            {
                return false;
            }
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Check root node

        // Scene configuration file must begin with `lightmetrica` node
        const auto* root = sceneConf->Root()->Child("lightmetrica");
        if (!root)
        {
            // TODO: Improve error messages
            LM_LOG_ERROR("Missing 'lightmetrica' node");
            return false;
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Scene version check

        {
            // Scene version support
            using VersionT = std::tuple<int, int, int>;
            const VersionT MinVersion{ 1, 0, 0 };
            const VersionT MaxVersion{ 1, 0, 0 };

            const auto* versionNode = root->Child("version");
            if (!versionNode)
            {
                LM_LOG_ERROR("Missing 'version' node");
                PropertyUtils::PrintPrettyError(root);
                return false;
            }

            // Parse version string
            const auto versionStr = versionNode->As<std::string>();
            std::regex re(R"x((\d)\.(\d)\.(\d))x");
            std::smatch match;
            const bool result = std::regex_match(versionStr, match, re);
            if (!result)
            {
                LM_LOG_ERROR("Invalid version string: " + versionStr);
                PropertyUtils::PrintPrettyError(versionNode);
                return false;
            }

            // Check version
            VersionT version{ std::stoi(match[1]), std::stoi(match[2]), std::stoi(match[3]) };
            if (version < MinVersion || MaxVersion < version)
            {
                {
                    LM_LOG_ERROR("Invalid version");
                    LM_LOG_INDENTER();
                    LM_LOG_ERROR(boost::str(boost::format("Expected: %d.%d.%d - %d.%d.%d")
                        % std::get<0>(MinVersion) % std::get<1>(MinVersion) % std::get<2>(MinVersion)
                        % std::get<0>(MaxVersion) % std::get<1>(MaxVersion) % std::get<2>(MaxVersion)));
                    LM_LOG_ERROR(boost::str(boost::format("Actual  : %d.%d.%d")
                        % std::get<0>(version) % std::get<1>(version) % std::get<2>(version)));
                }
                PropertyUtils::PrintPrettyError(versionNode);
                return false;
            }
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Initialize asset manager

        const auto assets = ComponentFactory::Create<Assets>();
        {
            LM_LOG_INFO("Initializing asset manager");
            LM_LOG_INDENTER();

            const auto* n = root->Child("assets");
            if (!n)
            {
                return false;
            }
            if (!assets->Initialize(n))
            {
                return false;
            }
        }
        
        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Initialize accel
        
        const auto accel = InitializeConfigurable<Accel>(root, "accel", { "qbvh" });
        if (!accel)
        {
            return false;
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Initialize scene

        const auto scene = ComponentFactory::Create<Scene>();
        {
            LM_LOG_INFO("Initializing scene");
            LM_LOG_INDENTER();

            const auto* n = root->Child("scene");
            if (!n)
            {
                return false;
            }
            if (!scene->Initialize(n, assets.get(), accel->get()))
            {
                return false;
            }
        }

        #pragma endregion

        // ---------------------------------------- ----------------------------------------

        #pragma region Initialize renderer

        const auto renderer = InitializeConfigurable<Renderer>(root, "renderer");
        if (!renderer)
        {
            return false;
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Process rendering

        {
            LM_LOG_INFO("Rendering");
            LM_LOG_INDENTER();
            const auto* sensor = static_cast<const Sensor*>(scene.get()->GetSensor()->emitter);
            auto* film = sensor->GetFilm();
            FPUtils::EnableFPControl();
            renderer.get()->Render(scene.get(), film);
            FPUtils::DisableFPControl();
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Save image

        {
            LM_LOG_INFO("Saving image");
            LM_LOG_INDENTER();
            auto* film = static_cast<const Sensor*>(scene.get()->GetSensor()->emitter)->GetFilm();
            if (!film->Save(opt.Render.OutputPath))
            {
                return false;
            }
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        return true;
    }

private:

    // Function to initialize configurable component
    template <typename AssetT>
    auto InitializeConfigurable(const PropertyNode* root, const std::string& name, const std::vector<std::string>& defs = {}) -> boost::optional<typename AssetT::UniquePtr>
    {
        static_assert(std::is_base_of<Configurable, AssetT>::value, "AssetT must inherits Configurable");

        LM_LOG_INFO("Initializing " + name);
        LM_LOG_INDENTER();

        const auto* n = root->Child(name);
        if (!n)
        {
            if (defs.empty())
            {
                LM_LOG_ERROR("Missing '" + name + "' node");
                PropertyUtils::PrintPrettyError(root);
                return boost::none;
            }

            LM_LOG_WARN("Missing '" + name + "' node");
            LM_LOG_INDENTER();

            for (const auto& def : defs)
            {
                LM_LOG_WARN("Using default type '" + def + "'");
                LM_LOG_INDENTER();
                auto p = ComponentFactory::Create<AssetT>(name + "::" + def);
                if (p == nullptr)
                {
                    LM_LOG_WARN("Failed to create '" + def + "'. Trying next candidate..");
                    continue;
                }
                return std::move(p);
            }

            LM_UNREACHABLE();
            return boost::none;
        }

        const auto tn = n->Child("type");
        if (!tn)
        {
            LM_LOG_ERROR("Missing '" + name + "/type' node");
            PropertyUtils::PrintPrettyError(n);
            return boost::none;
        }
        LM_LOG_INFO("Type: '" + tn->template As<std::string>() + "'");

        const auto pn = n->Child("params");
        //if (!pn)
        //{
        //    LM_LOG_ERROR("Missing '" + name + "/params' node");
        //    PropertyUtils::PrintPrettyError(n);
        //    return boost::none;
        //}

        auto p = ComponentFactory::Create<AssetT>(name + "::" + tn->template As<std::string>());
        if (!p)
        {
            LM_LOG_ERROR("Failed to create '" + tn->template As<std::string>() + "'");
            PropertyUtils::PrintPrettyError(tn);
            return boost::none;
        }

        if (!p->Initialize(pn))
        {
            LM_LOG_ERROR("Failed to initialize '" + tn->template As<std::string>() + "'");
            return boost::none;
        }

        return std::move(p);
    }

};

#pragma endregion

// --------------------------------------------------------------------------------

int main(int argc, char** argv)
{
    SEHUtils::EnableStructuralException();
    Logger::Run();

	int result = EXIT_SUCCESS;
	try
	{
        Application app;
		if (!app.Run(argc, argv))
		{
			result = EXIT_FAILURE;
		}
	}
	catch (const std::exception& e)
	{
        LM_LOG_ERROR_SIMPLE("EXCEPTION : " + std::string(e.what()));
		result = EXIT_FAILURE;
	}

	#if LM_DEBUG_MODE
    LM_LOG_INFO_SIMPLE("Press any key to exit ...");
    Logger::Flush();
	std::cin.get();
	#endif

    Logger::Stop();
    SEHUtils::DisableStructuralException();

	return result;
}


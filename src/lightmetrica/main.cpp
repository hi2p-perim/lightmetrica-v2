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
#include <lightmetrica/detail/propertyutils.h>
#include <lightmetrica/detail/stringtemplate.h>
#include <lightmetrica/detail/version.h>

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
    Verify,
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
                        ("scene,s", po::value<std::string>()->required(), "Scene description file")
                        ("output,o", po::value<std::string>()->default_value("result"), "Output image")
                        ("verbose,v", po::bool_switch()->default_value(false), "Adds detailed information on the output");

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

                    return true;
                }

                #pragma endregion
            
                // --------------------------------------------------------------------------------

                #pragma region Process verify subcommand

                if (subcmd == "verify")
                {
                    Type = SubcommandType::Verify;
                    return true;
                }

                #pragma endregion
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
            case SubcommandType::Verify: { return ProcessCommand_Verify(opt); }
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
        | Subcommands:
        | 
        |     - lightmetrica help
        |       Print global help message (this message).
        | 
        |     - lightmetrica verify
        |       Verification of the scene file.
        | 
        |     - lightmetrica render
        |       Render the image.
        |       `lightmetrica render --help` for more detailed help.
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
                #elif LM_PLATFORM_LINUX
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

        #pragma region Load configuration files

        // Scene configuration
        const auto sceneConf = ComponentFactory::Create<PropertyTree>();
        if (!sceneConf->LoadFromFile(opt.Render.SceneFile))
        {
            return false;
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
        
        const auto accel = InitializeConfigurable<Accel>(root, "accel", "naiveaccel");
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

        #pragma region Initialize film

        const auto film = InitializeConfigurable<Film>(root, "film");
        if (!film)
        {
            return false;
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
            renderer.get()->Render(scene.get(), static_cast<Film*>(film->get()));
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Save image

        {
            LM_LOG_INFO("Saving image");
            LM_LOG_INDENTER();
            if (!film.get()->Save(opt.Render.OutputPath))
            {
                return false;
            }
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        return true;
    }

    auto ProcessCommand_Verify(const ProgramOption& opt) -> bool
    {
        throw std::runtime_error("not implemented");
    }

private:

    // Function to initialize configurable component
    template <typename AssetT>
    auto InitializeConfigurable(const PropertyNode* root, const std::string& name, const std::string& default = "") -> boost::optional<typename AssetT::UniquePtr>
    {
        static_assert(std::is_base_of<Configurable, AssetT>::value, "AssetT must inherits Configurable");

        LM_LOG_INFO("Initializing " + name);
        LM_LOG_INDENTER();

        const auto* n = root->Child(name);
        if (!n)
        {
            if (!default.empty())
            {
                LM_LOG_WARN("Missing '" + name + "' node");
                LM_LOG_INDENTER();
                LM_LOG_WARN("Using default type: '" + default + "'");
                return ComponentFactory::Create<AssetT>(default);
            }
            else
            {
                LM_LOG_ERROR("Missing '" + name + "' node");
                PropertyUtils::PrintPrettyError(root);
                return boost::none;
            }
        }

        const auto tn = n->Child("type");
        if (!tn)
        {
            LM_LOG_ERROR("Missing '" + name + "/type' node");
            PropertyUtils::PrintPrettyError(n);
            return boost::none;
        }

        const auto pn = n->Child("params");
        if (!pn)
        {
            LM_LOG_ERROR("Missing '" + name + "/params' node");
            PropertyUtils::PrintPrettyError(n);
            return boost::none;
        }

        auto p = ComponentFactory::Create<AssetT>(tn->As<std::string>());
        if (!p)
        {
            LM_LOG_ERROR("Failed to initialize '" + tn->As<std::string>() + "'");
            PropertyUtils::PrintPrettyError(tn);
            return boost::none;
        }

        return p;
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


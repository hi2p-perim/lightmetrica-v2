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
#include <lightmetrica/renderjob.h>
#include <lightmetrica/assets.h>

#include <iostream>
#include <sstream>
#include <regex>

#include <boost/program_options.hpp>

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
        std::string RenderOptionFile;
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
                        ("render-option,o", po::value<std::string>()->required(), "Renderer option file");

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

                    Render.SceneFile = vm["scene"].as<std::string>();
                    Render.RenderOptionFile = vm["render-option"].as<std::string>();

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
            LM_LOG_INFO(MultiLineLiteral(R"x(
            |
            | Lightmetrica
            |
            | Copyright (c) 2015 Hisanari Otsu
            | The software is distributed under the MIT license.
            | For detail see the LICENSE file along with the software.
            |
            )x"));
        }

        #pragma endregion
        
        // --------------------------------------------------------------------------------

        #pragma region Load configuration files

        // Scene configuration
        const auto sceneConf = ComponentFactory::Create<PropertyTree>();
        if (!sceneConf->LoadFromString(opt.Render.SceneFile))
        {
            return false;
        }

        // Render configuration
        const auto renderConf = ComponentFactory::Create<PropertyTree>();
        if (!renderConf->LoadFromString(opt.Render.RenderOptionFile))
        {
            return false;
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Initialize scene

        // Asset manager
        const auto assets = ComponentFactory::Create<Assets>();

        // Scene
        const auto scene = std::move(ComponentFactory::Create<Scene>());
        if (!scene->Initialize(sceneConf->Root(), assets.get()))
        {
            return false;
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Initialize renderer

        const auto renderJob = std::move(ComponentFactory::Create<RenderJob>());
        if (!renderJob->Initialize(renderConf->Root()))
        {
            return false;
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Process rendering

        renderJob->Render(scene.get());

        #pragma endregion

        // --------------------------------------------------------------------------------

        return true;
    }

    auto ProcessCommand_Verify(const ProgramOption& opt) -> bool
    {
        throw std::runtime_error("not implemented");
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


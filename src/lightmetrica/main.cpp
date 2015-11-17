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

#include <iostream>

#include <boost/program_options.hpp>

using namespace lightmetrica_v2;

// --------------------------------------------------------------------------------

//bool Run(int argc, char** argv)
//{
//	#pragma region Parse arguments
//
//	namespace po = boost::program_options;
//
//	// Define options
//	po::options_description opt("Allowed options");
//	opt.add_options()
//		("help", "Display help message")
//		("scene,i", po::value<std::string>(), "Scene file")
//		("result,o", po::value<std::string>()->default_value("render.hdr"), "Rendered result")
//		("renderer,r", po::value<std::string>()->required(), "Rendering technique")
//		("num-samples,n", po::value<long long>()->default_value(10000000L), "Number of samples")
//		("max-num-vertices,m", po::value<int>()->default_value(-1), "Maximum number of vertices")
//		("width,w", po::value<int>()->default_value(1280), "Width of the rendered image")
//		("height,h", po::value<int>()->default_value(720), "Height of the rendered image")
//		("num-threads,j", po::value<int>(), "Number of threads")
//		#if LM_DEBUG_MODE
//		("grain-size", po::value<long long>()->default_value(10), "Grain size")
//		#else
//		("grain-size", po::value<long long>()->default_value(10000), "Grain size")
//		#endif
//		("progress-update-interval", po::value<long long>()->default_value(100000), "Progress update interval")
//		("render-time,t", po::value<double>()->default_value(-1), "Render time in seconds (-1 to use # of samples)")
//		("progress-image-update-interval", po::value<double>()->default_value(-1), "Progress image update interval (-1: disable)")
//		("progress-image-update-format", po::value<std::string>()->default_value("progress/{{count}}.png"), "Progress image update format string \n - {{count}}: image count");
//
//	// positional arguments
//	po::positional_options_description p;
//	p.add("renderer", 1).add("scene", 1).add("result", 1).add("width", 1).add("height", 1);
//
//	// Parse options
//	po::variables_map vm;
//	try
//	{
//		po::store(po::command_line_parser(argc, argv).options(opt).positional(p).run(), vm);
//		if (vm.count("help") || argc == 1)
//		{
//			std::cout << "Usage: nanogi [options] <renderer> <scene> <result> <width> <height>" << std::endl;
//			std::cout << opt << std::endl;
//			return 1;
//		}
//
//		po::notify(vm);
//	}
//	catch (po::required_option& e)
//	{
//		std::cerr << "ERROR : " << e.what() << std::endl;
//		return false;
//	}
//	catch (po::error& e)
//	{
//		std::cerr << "ERROR : " << e.what() << std::endl;
//		return false;
//	}
//
//	#pragma endregion
//
//	// --------------------------------------------------------------------------------
//
//	#pragma region Initial message
//
//	LM_LOG_INFO("Lightmetrica");
//	LM_LOG_INFO("Copyright (c) 2015 Hisanari Otsu");
//
//	#pragma endregion
//
//	// --------------------------------------------------------------------------------
//
//	//#pragma region Load scene
//
//	//Scene scene;
//	//{
//	//	LM_LOG_INFO("Loading scene");
//	//	LM_LOG_INDENTER();
//	//	if (!scene.Load(vm["scene"].as<std::string>(), (double)(vm["width"].as<int>()) / vm["height"].as<int>()))
//	//	{
//	//		return false;
//	//	}
//	//}
//
//	//#pragma endregion
//
//	// --------------------------------------------------------------------------------
//
//	//#pragma region Initialize renderer
//
//	//Renderer renderer;
//	//{
//	//	LM_LOG_INFO("Initializing renderer");
//	//	LM_LOG_INDENTER();
//	//	if (!renderer.Load(vm))
//	//	{
//	//		return false;
//	//	}
//	//}
//
//	//#pragma endregion
//
//	// --------------------------------------------------------------------------------
//
//	#pragma region Rendering
//
//	//std::vector<glm::dvec3> film;
//	//{
//	//	LM_LOG_INFO("Rendering");
//	//	LM_LOG_INDENTER();
//	//	renderer.Render(scene, film);
//	//}
//
//	//#pragma endregion
//
//	// --------------------------------------------------------------------------------
//
//	//#pragma region Save rendered image
//
//	//{
//	//	LM_LOG_INFO("Saving rendered image");
//	//	LM_LOG_INDENTER();
//	//	SaveImage(vm["result"].as<std::string>(), film, vm["width"].as<int>(), vm["height"].as<int>());
//	//}
//
//	//#pragma endregion
//
//	// --------------------------------------------------------------------------------
//
//	return true;
//}

// --------------------------------------------------------------------------------

int main(int argc, char** argv)
{
	LM_LOG_RUN();

	int result = EXIT_SUCCESS;
	//try
	//{
	//	#if LM_PLATFORM_WINDOWS
	//	_set_se_translator(SETransFunc);
	//	#endif
	//	if (!Run(argc, argv))
	//	{
	//		result = EXIT_FAILURE;
	//	}
	//}
	//catch (const std::exception& e)
	//{
 //       LM_LOG_ERROR("EXCEPTION | " + std::string(e.what()));
	//	result = EXIT_FAILURE;
	//}

	//#if LM_DEBUG_MODE
	//std::cerr << "Press any key to exit ...";
	//std::cin.get();
	//#endif

    LM_LOG_STOP();
	return result;
}


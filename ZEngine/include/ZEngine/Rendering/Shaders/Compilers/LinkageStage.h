#pragma once
#include <Rendering/Shaders/Compilers/ICompilerStage.h>

namespace ZEngine::Rendering::Shaders::Compilers
{

    class LinkageStage : public ICompilerStage
    {
    public:
        /**
         * Initialize a new LinkageStage instance.
         */
        LinkageStage();
        virtual ~LinkageStage();

        /**
         * Run Compiler stage
         *
         * @param information Collection of shader information
         */
        virtual void Run(std::vector<ShaderInformation>& information) override;

        /**
         * Run asynchronously compiler stage
         *
         * @param information Collection of shader information
         */
        virtual std::future<void> RunAsync(std::vector<ShaderInformation>& information) override;
    };
} // namespace ZEngine::Rendering::Shaders::Compilers

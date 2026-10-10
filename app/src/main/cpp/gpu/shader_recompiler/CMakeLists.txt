cmake_minimum_required(VERSION 3.22.1)

# ---------------------------------------------------------------------------
# PX5 shader recompiler — vendored from AnyPS5 core/shader/recompiler
# ---------------------------------------------------------------------------
# Provenance, license and the full adaptation ledger live in PX5_PORT.md next
# to this file. Source list mirrors AnyPS5's own CMakeLists (commit 262c0316)
# minus tests/DummyShaders.cpp (needs glslang, test-support only) and with
# SpirvOptimizer.cpp always compiled (guarded internally by PX5_SPIRV_TOOLS).
# SPIRV-Headers is vendored at cpp/third_party/spirv-headers (header-only).

set(PX5_SRC_ROOT "${CMAKE_CURRENT_LIST_DIR}")

add_library(px5_shader_recompiler STATIC
        Recompiler.cpp
        ShaderDiskCache.cpp
        SpirvBackend/src/RectListShaders.cpp
        SpirvBackend/src/BarycentricGeometryShader.cpp
        SpirvBackend/src/SpirvOptimizer.cpp
        RdnaDecoder/src/RdnaOpcode.cpp
        RdnaDecoder/src/RdnaProgram.cpp
        RdnaDecoder/src/RdnaInstructionDecoder.cpp
        RdnaDecoder/src/RdnaScalarOpDecoder.cpp
        RdnaDecoder/src/RdnaVectorOpDecoder.cpp
        RdnaDecoder/src/RdnaMemoryOpDecoder.cpp
        RdnaDecoder/src/RdnaImageOpDecoder.cpp
        RdnaDecoder/src/RdnaExportOpDecoder.cpp
        RdnaDecoder/src/RdnaDescriptorFormat.cpp
        ControlFlow/src/ControlFlowGraph.cpp
        ControlFlow/src/GraphBuilder.cpp
        ControlFlow/src/Structurizer.cpp
        ControlFlow/src/RequestSerializer.cpp
        IntermediateRepresentation/src/IrType.cpp
        IntermediateRepresentation/src/GuestRegister.cpp
        IntermediateRepresentation/src/IrOpcode.cpp
        IntermediateRepresentation/src/IrValue.cpp
        IntermediateRepresentation/src/IrBlock.cpp
        IntermediateRepresentation/src/IrProgram.cpp
        IntermediateRepresentation/src/IrBuilder/IrBuilderInternal.cpp
        IntermediateRepresentation/src/IrBuilder/IrBuilderCore.cpp
        IntermediateRepresentation/src/IrBuilder/IrBuilderConstants.cpp
        IntermediateRepresentation/src/IrBuilder/IrBuilderRegisters.cpp
        IntermediateRepresentation/src/IrBuilder/IrBuilderControlFlow.cpp
        IntermediateRepresentation/src/IrBuilder/IrBuilderExtendedRegisters.cpp
        IntermediateRepresentation/src/IrBuilder/IrBuilderConversions.cpp
        IntermediateRepresentation/src/IrBuilder/IrBuilderArithmetic.cpp
        IntermediateRepresentation/src/IrBuilder/IrBuilderLogic.cpp
        Translation/src/EmbeddedVertexFetch.cpp
        Translation/src/InstructionTranslator.cpp
        Translation/src/ShaderInputInfoBuilder.cpp
        Translation/src/TranslationContext/OperandAccess.cpp
        Translation/src/TranslationContext/SubwordPacking.cpp
        Translation/src/TranslationContext/ExecMask.cpp
        Translation/src/ScalarInstructions.cpp
        Translation/src/VectorInstructions.cpp
        Translation/src/FloatInstructions.cpp
        Translation/src/Float64Instructions.cpp
        Translation/src/IntegerInstructions.cpp
        Translation/src/CompareInstructions.cpp
        Translation/src/ControlFlowInstructions.cpp
        Translation/src/ConvertInstructions.cpp
        Translation/src/MemoryInstructions/MemoryResourceHelpers.cpp
        Translation/src/MemoryInstructions/ScalarBufferInstructions.cpp
        Translation/src/MemoryInstructions/BufferInstructions.cpp
        Translation/src/MemoryInstructions/FlatInstructions.cpp
        Translation/src/MemoryInstructions/ImageInstructions.cpp
        Translation/src/MemoryInstructions/SharedMemoryInstructions.cpp
        Translation/src/MemoryInstructions/MemoryInstructionDispatch.cpp
        Translation/src/AttributeInstructions.cpp
        Translation/src/DispatchInstructions.cpp
        Optimization/src/SsaBuilder/SsaBuilder.cpp
        Optimization/src/SsaBuilder/SsaVariable.cpp
        Optimization/src/SsaBuilder/SsaDefTable.cpp
        Optimization/src/SsaBuilder/SsaPass.cpp
        Optimization/src/SsaBuilder/SsaInstructionVisitor.cpp
        Optimization/src/SsaBuilder/SsaRegisterStateCleanup.cpp
        Optimization/src/ConstantFolder.cpp
        Optimization/src/DeadCodeEliminator.cpp
        Optimization/src/HostInterpolationChecker.cpp
        Optimization/src/ReadLaneEliminator.cpp
        Optimization/src/MaskedSelectEliminator.cpp
        Optimization/src/ResourceTracker.cpp
        Optimization/src/ResourceMaterializer.cpp
        Optimization/src/BindingAllocator.cpp
        Optimization/src/RequestMemoryView.cpp
        Optimization/src/DescriptorBindingBuilder.cpp
        Optimization/src/ShaderInfoCollector.cpp
        Optimization/src/SharedMemoryBarrierInserter.cpp
        Optimization/src/DenormalFlushEliminator.cpp
        Optimization/src/SrtWalker/SrtAddressArithmetic.cpp
        Optimization/src/SrtWalker/SrtDescriptorEvaluation.cpp
        Optimization/src/SrtWalker/SrtDiagnostics.cpp
        Optimization/src/SrtWalker/SrtEvaluator.cpp
        Optimization/src/SrtWalker/SrtFlatSlotClasses.cpp
        Optimization/src/SrtWalker/SrtInstructionPredicates.cpp
        Optimization/src/SrtWalker/SrtPlanBuilder.cpp
        Optimization/src/SrtWalker/SrtRuntimeValidator.cpp
        Optimization/src/SrtWalker/SrtWalker.cpp
        SpirvBackend/src/SpirvModule.cpp
        SpirvBackend/src/SpirvAnalysis.cpp
        SpirvBackend/src/SpirvAlu/SpirvAluEmitterMath.cpp
        SpirvBackend/src/SpirvAlu/SpirvAluEmitterFloat64.cpp
        SpirvBackend/src/SpirvAlu/SpirvAluEmitterRegisters.cpp
        SpirvBackend/src/SpirvFlowEmitter.cpp
        SpirvBackend/src/SpirvImageEmitter.cpp
        SpirvBackend/src/SpirvModuleEmitter.cpp
        SpirvBackend/src/SpirvBdaLookup.cpp
        SpirvBackend/src/SpirvBdaFault.cpp
        SpirvBackend/src/SpirvBdaRead.cpp
        SpirvBackend/src/SpirvEmitter.cpp
        SpirvBackend/src/SpirvBufferFormat.cpp
        SpirvBackend/src/SpirvRuntimeBuffer.cpp
        SpirvBackend/src/SpirvSpecialization.cpp
        SpirvBackend/src/SpirvMemory/SpirvBufferAccess.cpp
        SpirvBackend/src/SpirvMemory/SpirvConstants.cpp
        SpirvBackend/src/SpirvMemory/SpirvDescriptors.cpp
        SpirvBackend/src/SpirvMemory/SpirvFormatConvert.cpp
        SpirvBackend/src/SpirvMemory/SpirvInputOutput.cpp
        SpirvBackend/src/SpirvMemory/SpirvModuleSetup.cpp
        SpirvBackend/src/SpirvMemory/SpirvSubgroup.cpp
        SpirvBackend/src/SpirvMemory/SpirvTypes.cpp
        SpirvBackend/src/SpirvMemory/SpirvEmitterHelpers.cpp
        SpirvBackend/src/SpirvMemory/SpirvMemoryInstructions.cpp
        )

target_include_directories(px5_shader_recompiler PUBLIC
        ${PX5_SRC_ROOT}
        ${PX5_SRC_ROOT}/RdnaDecoder/include
        ${PX5_SRC_ROOT}/ControlFlow/include
        ${PX5_SRC_ROOT}/IntermediateRepresentation/include
        ${PX5_SRC_ROOT}/Translation/include
        ${PX5_SRC_ROOT}/Optimization/include
        ${PX5_SRC_ROOT}/SpirvBackend/include
        ${PX5_SRC_ROOT}/px5_shim
        ${CMAKE_CURRENT_LIST_DIR}/../../third_party/spirv-headers/include)

target_compile_features(px5_shader_recompiler PUBLIC cxx_std_20)
set_target_properties(px5_shader_recompiler PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        CXX_EXTENSIONS OFF)

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>

#include <regex>

#include "Pass/Graphviz.hpp"
#include "Pass/Util.hpp"

using namespace llvm;

namespace {

std::ofstream GetDefUseGraphOutstream(StringRef module_name) {
  auto filename =
      "def_use_" +
      std::regex_replace(module_name.str(), std::regex(R"(/)"), "_") + ".dot";

  return util::OpenFile(nullptr, filename.c_str());
}

std::ofstream GetControlFlowGraphOutstream(StringRef module_name) {
  auto filename =
      "control_flow_" +
      std::regex_replace(module_name.str(), std::regex(R"(/)"), "_") + ".dot";

  return util::OpenFile(nullptr, filename.c_str());
}

std::ofstream GetMemoryFlowGraphOutstream(StringRef module_name) {
  auto filename =
      "memory_flow_" +
      std::regex_replace(module_name.str(), std::regex(R"(/)"), "_") + ".dot";

  return util::OpenFile(nullptr, filename.c_str());
}

std::string GetInstrumentNPassesOutputFilename() {
  const char *filename = std::getenv("N_PASSES_EDGES");
  return filename ? filename : "n_passes_edges";
}

std::string GetInstrumentNUsageOutputFilename() {
  const char *filename = std::getenv("NODE_USAGE_COUNT");
  return filename ? filename : "node_usage_count";
}

std::string GetInstrumentMemoryOutputFile() {
  const char *filename = std::getenv("MEMORY_USAGE_PASS");
  return filename ? filename : "memory_usage";
}

uint64_t GetId(Value *value) { return reinterpret_cast<uint64_t>(value); }

std::string ExtractBBName(BasicBlock &BB) {
  std::string name;
  raw_string_ostream ss{name};
  BB.printAsOperand(ss);

  return name;
}

std::string ExtractIName(Instruction &I) {
  std::string name;
  raw_string_ostream ss{name};
  I.print(ss, true);

  return name;
}

bool IsInternal(Function &F) {
  return F.isIntrinsic() || F.getName().starts_with("__") ||
         F.getName().starts_with("_ZSt") || F.getName().starts_with("_ZNSt");
}

bool IsLogging(Function &F) {
  return F.getName() == "PrintNPassesEdges" ||
         F.getName() == "IncreaseNPasses" ||
         F.getName() == "PrepareIncreasePasses" || F.getName() == "AddUsage";
}

bool IsLogging(Module &M) { return M.getName().contains("FOR_LLVM"); }

// Control flow graph

struct ControlFlowBuilderPass : public PassInfoMixin<ControlFlowBuilderPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    if (IsLogging(M)) {
      return PreservedAnalyses::none();
    }

    dot::GraphvizBuilder graphviz(GetControlFlowGraphOutstream(M.getName()),
                                  false, false);

    CreateNodes(M, graphviz);
    CreateEdges(M, graphviz);
    InstrumentWithLogger(M);

    return PreservedAnalyses::all();
  }

private:
  // Creating nodes

  void CreateNodes(Module &M, dot::GraphvizBuilder &graphviz) {
    for (auto &F : M) {
      if (IsInternal(F) || IsLogging(F)) {
        continue;
      }

      auto func_subgraph = graphviz.StartSubgraph(GetId(&F), F.getName());
      graphviz.AddNode(GetId(&F), F.getName());
      for (auto &BB : F) {
        auto bb_name = ExtractBBName(BB);
        auto bb_subgraph = graphviz.StartSubgraph(GetId(&BB), bb_name);
        graphviz.AddNode(GetId(&BB), bb_name);

        for (auto &I : BB) {
          graphviz.AddNode(GetId(&I), ExtractIName(I));
        }
      }
    }
  }

  // Creating edges

  void ProceedInstructionFlow(Instruction &I, BasicBlock &BB,
                              dot::GraphvizBuilder &graphviz) {
    if (auto *call = dyn_cast<CallBase>(&I)) {
      Value *callee = call->getCalledOperand();
      assert(callee);

      auto *function_callee = dyn_cast<Function>(callee);
      if (!IsInternal(*function_callee) && !IsLogging(*function_callee)) {
        graphviz.AddEdge(GetId(&I), GetId(callee), kCallFlowColor);
      }
    }

    if (I.isTerminator()) {
      for (size_t successor_id = 0; successor_id < I.getNumSuccessors();
           successor_id++) {
        BasicBlock *successor = I.getSuccessor(successor_id);
        if (!successor) {
          continue;
        }
        graphviz.AddEdge(GetId(&I), GetId(successor), kTerminatorFlowColor);
      }
    } else if (I.getNextNode()) {
      graphviz.AddEdge(GetId(&I), GetId(I.getNextNode()), kNormalFlowColor);
    } else if (BB.getNextNode()) {
      graphviz.AddEdge(GetId(&I), GetId(BB.getNextNode()), kNormalFlowColor);
    }
  }

  void CreateEdges(Module &M, dot::GraphvizBuilder &graphviz) {
    for (auto &F : M) {
      if (F.isDeclaration() || IsInternal(F) || IsLogging(F)) {
        continue;
      }

      graphviz.AddEdge(GetId(&F), GetId(&F.front()), kNormalFlowColor);

      for (auto &BB : F) {
        graphviz.AddEdge(GetId(&BB), GetId(&BB.front()), kNormalFlowColor);

        for (auto &I : BB) {
          ProceedInstructionFlow(I, BB, graphviz);
        }
      }
    }
  }

  // Instrumenting

  FunctionCallee PrepareFunctionIncreaseNPasses(Module &M, LLVMContext &Ctx) {
    Type *ret_type = Type::getVoidTy(Ctx);
    Type *int64_type = Type::getInt64Ty(Ctx);

    FunctionType *funcIncreaseNPassesType =
        FunctionType::get(ret_type, {int64_type}, false);

    return M.getOrInsertFunction("IncreaseNPasses", funcIncreaseNPassesType);
  }

  FunctionCallee PrepareFunctionPrepareIncreasePasses(Module &M,
                                                      LLVMContext &Ctx) {
    Type *ret_type = Type::getVoidTy(Ctx);
    Type *int64_type = Type::getInt64Ty(Ctx);

    FunctionType *funcPrepareIncreasePassesType =
        FunctionType::get(ret_type, {int64_type}, false);
    return M.getOrInsertFunction("PrepareIncreasePasses",
                                 funcPrepareIncreasePassesType);
  }

  void InstrumentMain(Function &F, IRBuilder<> &builder, LLVMContext &Ctx,
                      Module &M) {
    Type *ret_type = Type::getVoidTy(Ctx);
    Type *ptr_type = PointerType::get(Ctx, 0);

    assert(F.getName() == "main");

    FunctionType *printNPassesEdgesType =
        FunctionType::get(ret_type, {ptr_type}, false);
    FunctionCallee printNPassesEdges =
        M.getOrInsertFunction("PrintNPassesEdges", printNPassesEdgesType);

    builder.SetInsertPoint(&F.back().back());
    Value *funcName =
        builder.CreateGlobalString(GetInstrumentNPassesOutputFilename());
    Value *args[] = {funcName};
    builder.CreateCall(printNPassesEdges, args);
  }

  void InstrumentBasicBlock(BasicBlock &BB, uint64_t to_node_id,
                            IRBuilder<> &builder, Module &M, LLVMContext &Ctx) {
    Instruction *insert_point = &*BB.getFirstNonPHIOrDbgOrLifetime();
    if (isa<LandingPadInst>(insert_point)) {
      return;
    }

    Type *int64_type = Type::getInt64Ty(Ctx);

    builder.SetInsertPoint(insert_point);
    Value *to_node_value_id = ConstantInt::get(int64_type, to_node_id);
    Value *args[] = {to_node_value_id};
    builder.CreateCall(PrepareFunctionIncreaseNPasses(M, Ctx), args);
  }

  void InstrumentInstruction(Instruction &I, uint64_t from_node_id,
                             IRBuilder<> &builder, Module &M,
                             LLVMContext &Ctx) {
    auto *call = dyn_cast<CallBase>(&I);
    if (!I.isTerminator() && !call) {
      return;
    }

    if (call && IsLogging(*call->getCalledFunction())) {
      return;
    }

    builder.SetInsertPoint(&I);
    Type *int64_type = Type::getInt64Ty(Ctx);
    Value *from_node_id_value = ConstantInt::get(int64_type, from_node_id);
    Value *from_args[] = {from_node_id_value};

    if (call) {
      Value *to_node_id_value =
          ConstantInt::get(int64_type, GetId(call->getCalledFunction()));
      Value *to_args[] = {to_node_id_value};

      builder.CreateCall(PrepareFunctionPrepareIncreasePasses(M, Ctx),
                         from_args);
      builder.CreateCall(PrepareFunctionIncreaseNPasses(M, Ctx), to_args);

      // handle after call-return res
      auto *after_call = call->getNextNode();
      assert(after_call);
      builder.SetInsertPoint(after_call);
      builder.CreateCall(PrepareFunctionIncreaseNPasses(M, Ctx), from_args);

      return;
    }

    builder.SetInsertPoint(&I);
    builder.CreateCall(PrepareFunctionPrepareIncreasePasses(M, Ctx), from_args);
  }

  void InstrumentWithLogger(Module &M) {
    LLVMContext &Ctx = M.getContext();
    IRBuilder<> builder(Ctx);

    for (auto &F : M) {
      if (F.isDeclaration() || IsInternal(F)) {
        continue;
      }

      if (F.getName() == "main") {
        InstrumentMain(F, builder, Ctx, M);
      }

      if (IsLogging(F)) {
        continue;
      }

      for (auto &BB : F) {
        InstrumentBasicBlock(BB, GetId(&BB), builder, M, Ctx);
        for (auto &I : BB) {
          InstrumentInstruction(I, GetId(&I), builder, M, Ctx);
        }
      }
    }
  }

private:
  static constexpr auto kNormalFlowColor = dot::GraphvizBuilder::Color::Black;
  static constexpr auto kCallFlowColor = dot::GraphvizBuilder::Color::Blue;
  static constexpr auto kTerminatorFlowColor =
      dot::GraphvizBuilder::Color::Blue;
};

// Def-use graph

struct DefUseBuilderPass : public PassInfoMixin<DefUseBuilderPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    if (IsLogging(M)) {
      return PreservedAnalyses::none();
    }

    dot::GraphvizBuilder graphviz(GetDefUseGraphOutstream(M.getName()), false,
                                  false);

    BuildStaticGraph(M, graphviz);
    InstrumentWithLogger(M);

    return PreservedAnalyses::all();
  }

private:
  // Build static graph
  bool Exists(uint64_t id) { return existent_nodes_.count(id); }

  bool NodeExists(Value &value) { return Exists(GetId(&value)); }

  void AddNodeIfNoneExistent(Value &value, std::string_view name,
                             dot::GraphvizBuilder &graphviz) {
    uint64_t id = GetId(&value);
    if (Exists(id)) {
      return;
    }

    existent_nodes_.insert(id);

    graphviz.AddNode(id, name);
  }

  uint64_t AddNewUniqueNode(std::string_view name,
                            dot::GraphvizBuilder &graphviz) {
    static uint64_t node_id = 0;
    assert(existent_nodes_.count(node_id) == 0);

    graphviz.AddNode(node_id, name);
    return node_id++;
  }

  void ProceedInstructionFlow(Instruction &I, dot::GraphvizBuilder &graphviz) {
    auto *call = dyn_cast<CallBase>(&I);
    if (call) {
      auto *callee = dyn_cast<Function>(call->getCalledFunction());
      if (callee && IsLogging(*callee)) {
        return;
      }
    }

    std::string name;
    raw_string_ostream ss{name};

    if (!I.operands().empty()) {
      name.clear();
      I.print(ss);
      AddNodeIfNoneExistent(I, name, graphviz);
    }

    for (auto &U : I.operands()) {
      name.clear();

      Value *use = U.get();

      if (dyn_cast<Constant>(use)) {
        use->printAsOperand(ss);
        uint64_t node_id = AddNewUniqueNode(name, graphviz);
        graphviz.AddEdge(node_id, GetId(&I), kDefUseColor);
        continue;
      }

      if (dyn_cast<Instruction>(use)) {
        use->print(ss);
      } else {
        use->printAsOperand(ss);
      }
      AddNodeIfNoneExistent(*use, name, graphviz);
      graphviz.AddEdge(GetId(use), GetId(&I), kDefUseColor);
    }
  }

  void BuildStaticGraph(Module &M, dot::GraphvizBuilder &graphviz) {
    for (auto &F : M) {
      auto func_subgraph = graphviz.StartSubgraph(GetId(&F), F.getName());
      for (auto &BB : F) {
        auto bb_subgraph =
            graphviz.StartSubgraph(GetId(&BB), ExtractBBName(BB));
        for (auto &I : BB) {
          ProceedInstructionFlow(I, graphviz);
        }
      }
    }
  }

  // Instrument graph

  void InstrumentMain(Function &F, Module &M, LLVMContext &Ctx,
                      IRBuilder<> &builder) {
    Type *ret_type = Type::getVoidTy(Ctx);
    Type *ptr_type = PointerType::get(Ctx, 0);

    assert(F.getName() == "main");

    FunctionType *printNUsagesType =
        FunctionType::get(ret_type, {ptr_type}, false);
    FunctionCallee printNUsages =
        M.getOrInsertFunction("PrintUsages", printNUsagesType);

    builder.SetInsertPoint(&F.back().back());
    Value *funcName =
        builder.CreateGlobalString(GetInstrumentNUsageOutputFilename());
    Value *args[] = {funcName};
    builder.CreateCall(printNUsages, args);
  }

  void InstrumentInstruction(Instruction &I, Module &M, LLVMContext &Ctx,
                             IRBuilder<> &builder) {
    if (!NodeExists(I)) {
      return;
    }
    Instruction *insert_point = &I;

    if (isa<PHINode>(I) || isa<LandingPadInst>(I)) {
      insert_point = I.getNextNode();
      assert(insert_point);
    }

    Type *ret_type = Type::getVoidTy(Ctx);
    Type *int64_type = Type::getInt64Ty(Ctx);

    FunctionType *funcAddUsageType =
        FunctionType::get(ret_type, {int64_type}, false);
    FunctionCallee funcAddUsage =
        M.getOrInsertFunction("AddUsage", funcAddUsageType);

    builder.SetInsertPoint(insert_point);
    Value *node_id = ConstantInt::get(int64_type, GetId(&I));
    Value *args[] = {node_id};

    builder.CreateCall(funcAddUsage, args);
  }

  void InstrumentWithLogger(Module &M) {
    LLVMContext &Ctx = M.getContext();
    IRBuilder<> builder{Ctx};

    for (auto &F : M) {
      if (IsLogging(F) || IsInternal(F)) {
        continue;
      }

      if (F.getName() == "main") {
        InstrumentMain(F, M, Ctx, builder);
      }

      for (auto &&BB : F) {
        for (auto &I : BB) {
          InstrumentInstruction(I, M, Ctx, builder);
        }
      }
    }
  }

private:
  std::set<uint64_t> existent_nodes_;

  static constexpr auto kDefUseColor = dot::GraphvizBuilder::Color::Black;
};

// Memory alloc pass

struct MemoryAllocPass : public PassInfoMixin<MemoryAllocPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    if (IsLogging(M)) {
      return PreservedAnalyses::none();
    }

    dot::GraphvizBuilder graphviz{GetMemoryFlowGraphOutstream(M.getName()),
                                  false, false};

    CreateNodes(M, graphviz);

    LLVMContext &Ctx = M.getContext();
    IRBuilder<> builder{Ctx};

    for (auto &F : M) {
      if (F.getName() == "main") {
        InstrumentMain(F, M, Ctx, builder);
      }

      for (auto &BB : F) {
        for (auto &I : BB) {
          InstrumentInstruction(I, M, Ctx, builder);
        }
      }
    }

    return PreservedAnalyses::all();
  }

private:
  // Create nodes

  void CreateNodes(Module &M, dot::GraphvizBuilder &graphviz) {
    for (auto &F : M) {
      if (IsInternal(F) || IsLogging(F)) {
        continue;
      }

      auto func_subgraph = graphviz.StartSubgraph(GetId(&F), F.getName());
      graphviz.AddNode(GetId(&F), F.getName());
      for (auto &BB : F) {
        auto bb_name = ExtractBBName(BB);
        auto bb_subgraph = graphviz.StartSubgraph(GetId(&BB), bb_name);
        graphviz.AddNode(GetId(&BB), bb_name);

        for (auto &I : BB) {
          graphviz.AddNode(GetId(&I), ExtractIName(I));
        }
      }
    }
  }

  // Instrument memory
  void InstrumentMain(Function &F, Module &M, LLVMContext &Ctx,
                      IRBuilder<> &builder) {
    Type *ret_type = Type::getVoidTy(Ctx);
    Type *ptr_type = PointerType::get(Ctx, 0);

    assert(F.getName() == "main");

    FunctionType *printNPassesEdgesType =
        FunctionType::get(ret_type, {ptr_type}, false);
    FunctionCallee printNPassesEdges = M.getOrInsertFunction(
        "PrintAllocatedMemoryInfo", printNPassesEdgesType);

    builder.SetInsertPoint(&F.back().back());
    Value *funcName =
        builder.CreateGlobalString(GetInstrumentMemoryOutputFile());
    Value *args[] = {funcName};
    builder.CreateCall(printNPassesEdges, args);
  }

  Value *GetInstructionValueId(Instruction &I, LLVMContext &Ctx) {
    Value *name_id = ConstantInt::get(Type::getInt64Ty(Ctx), GetId(&I));

    return name_id;
  }

  bool HandleMemAllocCall(Instruction &I, Module &M, LLVMContext &Ctx,
                          IRBuilder<> &builder) {
    if (!isa<CallBase>(&I)) {
      return false;
    }

    CallBase *call = cast<CallBase>(&I);
    Function *calledFunc = call->getCalledFunction();

    if (!calledFunc) {
      return false;
    }

    StringRef funcName = calledFunc->getName();
    if (funcName != "malloc" && funcName != "calloc") {
      return false;
    }

    assert(call->getNextNode());
    builder.SetInsertPoint(call->getNextNode());

    Value *allocated_ptr = call;

    FunctionCallee addMemFunc = M.getOrInsertFunction(
        "AddDynamicallyAllocatedMemory", Type::getVoidTy(Ctx),
        Type::getInt64Ty(Ctx), allocated_ptr->getType());

    Value *name_id = GetInstructionValueId(I, Ctx);
    builder.CreateCall(addMemFunc, {name_id, allocated_ptr});

    return true;
  }

  bool HandleMemRealloc(Instruction &I, Module &M, LLVMContext &Ctx,
                        IRBuilder<> &builder) {
    if (!isa<CallBase>(&I)) {
      return false;
    }

    CallBase *call = cast<CallBase>(&I);
    Function *calledFunc = call->getCalledFunction();

    if (!calledFunc) {
      return false;
    }

    StringRef funcName = calledFunc->getName();
    if (funcName != "realloc") {
      return false;
    }

    assert(call->getNextNode());
    builder.SetInsertPoint(call->getNextNode());

    Value *deallocated_ptr = call->getArgOperand(0);
    Value *allocated_ptr = call;

    FunctionCallee deallocMemFunc = M.getOrInsertFunction(
        "RemoveDynamicallAllocatedMemory", Type::getVoidTy(Ctx),
        Type::getInt64Ty(Ctx), deallocated_ptr->getType());

    FunctionCallee addMemFunc = M.getOrInsertFunction(
        "AddDynamicallyAllocatedMemory", Type::getVoidTy(Ctx),
        Type::getInt64Ty(Ctx), allocated_ptr->getType());

    Value *name_id = GetInstructionValueId(I, Ctx);
    builder.CreateCall(deallocMemFunc, {name_id, deallocated_ptr});
    builder.CreateCall(addMemFunc, {name_id, allocated_ptr});

    return true;
  }

  bool HandleMemFreeCall(Instruction &I, Module &M, LLVMContext &Ctx,
                         IRBuilder<> &builder) {
    if (!isa<CallBase>(&I)) {
      return false;
    }

    CallBase *call = cast<CallBase>(&I);
    Function *calledFunc = call->getCalledFunction();
    if (!calledFunc) {
      return false;
    }

    if (calledFunc->getName() != "free") {
      return false;
    }

    builder.SetInsertPoint(call);
    Value *freed_ptr = call->getArgOperand(0);

    FunctionCallee removeMemFunc = M.getOrInsertFunction(
        "RemoveDynamicallAllocatedMemory", Type::getVoidTy(Ctx),
        Type::getInt64Ty(Ctx), freed_ptr->getType());

    Value *name_id = GetInstructionValueId(I, Ctx);
    builder.CreateCall(removeMemFunc, {name_id, freed_ptr});
    return true;
  }

  void InstrumentInstruction(Instruction &I, Module &M, LLVMContext &Ctx,
                             IRBuilder<> &builder) {
    if (HandleMemAllocCall(I, M, Ctx, builder)) {
      return;
    }
    if (HandleMemFreeCall(I, M, Ctx, builder)) {
      return;
    }
    if (HandleMemRealloc(I, M, Ctx, builder)) {
      return;
    }

    if (isa<CallBase>(I)) {
      return;
    }

    for (ssize_t i = 0; i < I.getNumOperands(); ++i) {
      Value *op = I.getOperand(i);
      if (op->getType()->isPointerTy()) {
        builder.SetInsertPoint(&I);

        FunctionCallee logFunc = M.getOrInsertFunction(
            "LogIfMemoryIsDynamicallyAllocated", Type::getVoidTy(Ctx),
            Type::getInt64Ty(Ctx), op->getType());

        Value *name_id = GetInstructionValueId(I, Ctx);
        builder.CreateCall(logFunc, {name_id, op});
      }
    }
  }
};

PassPluginLibraryInfo getPassPluginInfo() {
  const auto callback = [](PassBuilder &PB) {
    PB.registerPipelineStartEPCallback([=](ModulePassManager &MPM, auto) {
      MPM.addPass(ControlFlowBuilderPass{});
      MPM.addPass(DefUseBuilderPass{});
      MPM.addPass(MemoryAllocPass{});
      return true;
    });
  };

  return {LLVM_PLUGIN_API_VERSION, "MyPlugin", "0.0.1", callback};
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getPassPluginInfo();
}

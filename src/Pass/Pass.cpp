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

std::ofstream GetDefUseGraphOutstream() {
  return util::OpenFile("GRAPHVIZ_DEF_USE", "def_use.dot");
}

std::ofstream GetControlFlowGraphOutstream(StringRef module_name) {
  auto filename =
      "control_flow_" +
      std::regex_replace(module_name.str(), std::regex(R"(/)"), "_") + ".dot";
  return util::OpenFile(nullptr, filename.c_str());
}

std::string GetInstrumentNPassesOutputFilename() {
  const char *filename = std::getenv("INSTRUMENT_N_PASSES");
  return filename ? filename : "n_passes_edges";
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
         F.getName() == "PrepareIncreasePasses";
}

struct ControlFlowBuilderPass : public PassInfoMixin<ControlFlowBuilderPass> {
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    if (M.getName().find("FOR_LLVM") != std::string::npos) {
      return PreservedAnalyses::none();
    }

    dot::GraphvizBuilder graphviz(GetControlFlowGraphOutstream(M.getName()), false, false);

    CreateNodes(M, graphviz);
    CreateEdges(M, graphviz);
    InstrumentWithLogger(M);

    M.print(outs(), nullptr);
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
    Type *retType = Type::getVoidTy(Ctx);
    Type *int64Ty = Type::getInt64Ty(Ctx);

    FunctionType *funcIncreaseNPassesType =
        FunctionType::get(retType, {int64Ty}, false);

    return M.getOrInsertFunction("IncreaseNPasses", funcIncreaseNPassesType);
  }

  FunctionCallee PrepareFunctionPrepareIncreasePasses(Module &M,
                                                      LLVMContext &Ctx) {
    Type *retType = Type::getVoidTy(Ctx);
    Type *int64Ty = Type::getInt64Ty(Ctx);

    FunctionType *funcPrepareIncreasePassesType =
        FunctionType::get(retType, {int64Ty}, false);
    return M.getOrInsertFunction("PrepareIncreasePasses",
                                 funcPrepareIncreasePassesType);
  }

  void InstrumentMain(Function &F, IRBuilder<> &builder, LLVMContext &Ctx,
                      Module &M) {
    Type *retType = Type::getVoidTy(Ctx);
    Type *ptrType = PointerType::get(Ctx, 0);
    Type *int64Ty = Type::getInt64Ty(Ctx);

    assert(F.getName() == "main");

    FunctionType *printNPassesEdgesType =
        FunctionType::get(retType, {ptrType}, false);
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

    Type *int64Ty = Type::getInt64Ty(Ctx);

    builder.SetInsertPoint(insert_point);
    Value *to_node_value_id = ConstantInt::get(int64Ty, to_node_id);
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
    Type *int64Ty = Type::getInt64Ty(Ctx);
    Value *from_node_id_value = ConstantInt::get(int64Ty, from_node_id);
    Value *from_args[] = {from_node_id_value};

    if (call) {
      Value *to_node_id_value =
          ConstantInt::get(int64Ty, GetId(call->getCalledFunction()));
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

    outs() << "Instrument instruction " << "\n";
    I.print(outs(), true);
    outs() << "\n";

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
  static constexpr auto kTerminatorFlowColor = dot::GraphvizBuilder::Color::Blue;
};

struct DefUseBuilderPass : public PassInfoMixin<DefUseBuilderPass> {

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    dot::GraphvizBuilder graphviz(GetDefUseGraphOutstream(), false, false);

    return PreservedAnalyses::all();
  }
};

#if 0
struct DefUseBuilderPass : public PassInfoMixin<DefUseBuilderPass> {

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    dot::GraphvizBuilder graphviz(GetDefUseGraphOutstream());

    for (auto &F : M) {
      auto func_subraph = graphviz.StartSubgraph(F.getName(), F.getName());
      graphviz.AddNode(GetId(&F), F.getName());

      for (auto &BB : F) {
        // auto bb_subgraph = graphviz.StartSubgraph("BB");
        graphviz.AddNode(GetId(&BB), "Basic Block");
        for (auto &I : BB) {
          std::string name;
          raw_string_ostream ss{name};
          I.print(ss, true);
          graphviz.AddNode(GetId(&I), name);
        }
      }
    }

    for (auto &F : M) {
      for (auto &U : F.uses()) {
        User *user = U.getUser();
        graphviz.AddEdge(GetId(&F), GetId(user),
                         dot::GraphvizBuilder::Color::Green);
      }

      for (auto &BB : F) {
        for (auto &U : BB.uses()) {
          User *user = U.getUser();
          graphviz.AddEdge(GetId(&BB), GetId(user),
                           dot::GraphvizBuilder::Color::Green);
        }
        for (auto &I : BB) {
          for (auto &U : I.uses()) {
            User *user = U.getUser();
            graphviz.AddEdge(GetId(&I), GetId(user),
                             dot::GraphvizBuilder::Color::Green);
          }
        }
      }
    }
#if 0
        for (auto& F : M) {
            auto subgraph = graphviz.StartSubgraph(F.getName());

            outs() << "Begin args\n";
            for (auto& arg : F.args()) {
                outs() << "Arg with addr " << GetId(&arg) << "and name " << arg.getName() << "\n";
                graphviz.AddNode(GetId(&arg), arg.getName());    
                outs() << "\n";
            }
            outs() << "End args\n";

            for (auto& BB : F) {
                for (auto& I : BB) {
                    std::string instruction_name;
                    raw_string_ostream ss{instruction_name};
                    I.print(ss, true);

                    outs() << "Current instruction with addr " << GetId(&I) << "\n";
                    I.print(outs(), true);
                    outs() << "\n";
                    graphviz.AddNode(GetId(&I), instruction_name);
                    for (auto& U : I.operands()) {
                        Value* value = U.get();

                        if (!dyn_cast<Instruction>(value)) {
                            continue;
                        }
                        outs() << "Current operand with addr " << GetId(value) << "\n";
                        U->print(outs(), true);
                        outs() << "\n";
                        graphviz.AddEdge(GetId(U), GetId(&I), dot::GraphvizBuilder::Color::Red);
                    }
                }
            }
        }
#endif

    return PreservedAnalyses::all();
  }
};

#endif

PassPluginLibraryInfo getPassPluginInfo() {
  const auto callback = [](PassBuilder &PB) {
    PB.registerOptimizerLastEPCallback([=](ModulePassManager &MPM, auto, auto) {
      MPM.addPass(DefUseBuilderPass{});
      MPM.addPass(ControlFlowBuilderPass{});
      return true;
    });
  };

  return {LLVM_PLUGIN_API_VERSION, "MyPlugin", "0.0.1", callback};
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getPassPluginInfo();
}

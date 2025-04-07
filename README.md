# LLVM PASS

## Description

This project contains 3 LLVM Passes that are used for debugging and potentially optimization purposes: 

- [Def use graph builder](#def-use-pass)
- [Control flow graph builder](#control-flow-pass)
- [Memory allocation / use graph builder](#memory-alloc-use-pass)

Using LLVM Pass, static graphs that represent dependencies between instructions are built during compile-time. Furthermore, code is instrumented with logging functions that provide dynamic information in runtime. For example, the def/use graph can be built using only static information. And dynamic logging helps to answer various questions such as:
1. How often particular branch is taken?
2. How often is this instruction executed? 
And many other questions that couldn't be answered at compile-time.

If program consists of different translation units, graphs generated from different units are split. It is useful, because this way graph representations are not too cluttered.

## Install and Run

```
git clone https://github.com/d3clane/llvm_pass.git
mkdir build && cd build && cmake ..
make
```

// TODO: memory usage graph

These commands generate the executable `a.out` that you can now run. After running it, dynamic information is collected and stored into files with names `n_passes_edges` for control flow graph and `node_usage_count` for def_use graph. In order to combine static graphs with dynamic info you should run:

```
./ConcatDU node_usage_count def_use out_file_name
./ConcatCF n_passes_edges control_flow out_file_name
```

If you'd like to see only static information, concatenate it with empty_file:

```
./ConcatDU empty_file def_use out_file_name
./ConcatCF empty_file control_flow out_file_name
```

Pngs will be generated and stored in build/png/.

## Def Use Pass

The def-use representation is a format in which nodes are functions, basic blocks, instructions or constants and edge from node1 to node2 is held if node2 uses node1 as an operand. Examples have been compiled without optimization, otherwise it would change significantly. Here's a simple one: 
```C
int func(int val1, int val2) {
   while (val1 < 10) {
    ++val1;
   }

   while (val2 < 10) {
    ++val2;
   }

   return val1 + val2;
}

int main() {
    func(0, 5);
}
```

Results in this graph:

![def_use_test1](ReadmeAssets/imgs/def_use_test1.png)

It's quite hard to see, so here's part of this graph:

![def_use_test1_part](ReadmeAssets/imgs/def_use_test1_part.png)

Now, we can add runtime information to this graph. For now, it is instrumented with `CountUsage` function that for each node in def/use graph count how many times this instruction is executed during program. After that using script nodes are colored in something between green and red based on instruction utilization (more "red" means more executions). Here's the result:

![def_use_test1_dyn](ReadmeAssets/imgs/def_use_test1_dyn.png)

And here's some parts of this graph:

![def_use_test1_dyn_part1](ReadmeAssets/imgs/def_use_test1_dyn_part1.png)  
![def_use_test1_dyn_part2](ReadmeAssets/imgs/def_use_test1_dyn_part2.png)

We can see that some of the nodes are red - meaning that they are executed most frequently. There are also green ones - meaning they are executed rarely. And there are even in-between colored (something between green and red) nodes that indicate that they are executed not the most frequent, but also not rare.

Now, for better understanding of flow of data and control, let's see how control flow pass works.

## Control Flow Pass

In control flow graph nodes are still functions, basic blocks and instructions. An edge from node1 to node2 exists at compile time if after executing node1 the next executing instruction could be node2. For instance, edge from `call` to `called function` is drawn. Or, two edges from a `branch to label1 or label2` are created to `label1` and `label2`. Here is the representation graph for the same program:

<img src="ReadmeAssets/imgs/control_flow_test1.png" width="50%" />

Blue edges mean "non-linear" change of control-flow (branches, calls, etc.).

Now, moving on to dynamic information. It's impossible to say at compile-time, how many times will specific branch be taken, which branch is taken more often, or, for example, how many times function is called from this `call` instruction. The more frequent is particular jump, the thicker and "more red" becomes the edge. There are also numbers written on edges that indicate frequency of jump. Here's the result: 

<img src="ReadmeAssets/imgs/control_flow_test1_dyn.png" width="50%" />

And one of the components of this graph:

<img src="ReadmeAssets/imgs/control_flow_test1_dyn_part.png" width="50%" />

And this image perfectly matches the previous def/use graph. These two representations are very convenient for using together.

## Memory Alloc Use Pass

// TODO: 

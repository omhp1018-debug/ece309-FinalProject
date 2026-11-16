# ECE 309 Final Project Design Log

> The P2 and P3 code in this tree came from my submitted project archives. I wrote the final-project implementation myself.

## AI and tool use

On November 16, 2026, I used ChatGPT to help debug and check my code. It compiled the project with g++ using C++17, warnings-as-errors, AddressSanitizer, and UndefinedBehaviorSanitizer, then ran the existing final, P2, P3 tool-loop, and end-to-end tests. All tests passed. ChatGPT also helped revise this design log; it did not change project source files during this review.

## LRU cache

The cache uses an `unordered_map<Key, Node*>` to locate nodes and a doubly linked list to store recency. The head is most recently used; the tail is evicted first. On a `get` hit for a middle node `X`, the code writes `P->next = N`, `N->prev = P`, `X->prev = nullptr`, `X->next = old_head`, `old_head->prev = X`, and `head_ = X`. Each write is a fixed pointer assignment. The map lookup is expected O(1), so the whole hit is expected O(1), regardless of the list length.

For an eviction during `put`, it saves the tail key, changes `tail_` to `victim->prev`, sets the new tail's `next` to null (or clears `head_` if this was the only node), erases the saved key from the map, and deletes the old node. It then allocates the new node, points it at the old head, changes the old head's `prev` (or sets `tail_` for the empty case), sets `head_`, and inserts the map entry. These are a fixed number of operations. The capacity-zero and capacity-one cases use the same list helpers and explicit capacity check.

## Heap and top-k

`MinHeap` stores items in a vector and performs its own sift-up and sift-down. `top_k` retains at most ten candidates for the requested workload. A new item that is not better than the root is skipped; a better one replaces the current worst candidate. That means at most `n` heap comparisons/updates, each O(log k), for O(n log k) time and O(k) auxiliary space. Sorting all `n` values is O(n log n). In this instrumented run, `top_k` on 100,000 random integers with k=10 took 25.207 ms; sorting the full vector in descending order took 167.245 ms. These are one-run timings, not a hardware-independent guarantee.

## BST

The printed heights after inserting ids 1 through 1,000 were 1,000 in order and 25 in a shuffled order. Sequential inserts always follow the right child, so the unbalanced BST becomes a chain. `find` then takes O(n) in the worst case rather than O(log n). A self-balancing tree such as AVL would bound the height logarithmically; this reference keeps the required core BST behavior and does not include the optional AVL bonus.

## Graph traversal

DFS uses white (unseen), gray (on the active recursion path), and black (finished). An edge to gray is a back-edge and proves a cycle. An edge to black only means a dependency has already been processed. In the diamond `a -> b`, `a -> c`, `b -> d`, `c -> d`, a two-color visited/unvisited algorithm might call the second visit to `d` a cycle even though `d` was completed through `b`; three colors distinguish that harmless completed node from a node still on the path. DFS appends each skill after its requirements, producing dependency-first load order. The BFS for dependents walks reversed edges and ranks by distance, then name.

## Inverted-index timing

The test generated documents containing a common term, a rotating token, and a random group token. It averaged 100 queries for `common group7`; times are microseconds per query under the sanitizer build.

| Documents | Inverted index | Linear scan |
|---:|---:|---:|
| 100 | 226 | 979 |
| 1,000 | 2,803 | 13,459 |
| 5,000 | 7,682 | 54,176 |

The index still grows with the posting lists for query terms, but it avoids retokenizing every document per query. The linear scan grows with the full corpus. The index is faster at all three sizes, though the measurements depend on the generated data and machine.

## Command-policy threat model

The default chain denies commands containing configured shell metacharacters or risky substrings, then checks a program allowlist, asks for confirmation for selected programs, and limits the number of allowed requests in a sliding time window. The command tool is simulated and never launches a process. This is a narrow policy for a teaching harness, not a shell sandbox: for example, `python3` is explicitly allowlisted and can express behavior the string filters do not recognize. The substring checks can also miss obfuscation and are not a substitute for operating-system isolation.

## Validation and hindsight

The archive was compiled as C++17 with `-Wall -Wextra -Wpedantic -Werror` and AddressSanitizer/UndefinedBehaviorSanitizer. The 22 final tests, 14 P2 tests, 9 P3 tool-loop tests, and 11 end-to-end sessions passed in this review. CMake was unavailable in the review environment, so the targets were compiled directly with g++; leak detection was disabled because LeakSanitizer could not run under the environment's ptrace setup. One design choice I would revisit is the unbalanced memory BST: sequential ids make its performance poor by construction. In a production memory store I would choose a balanced tree or a database index, while keeping the ordered traversal and removal semantics tested here.

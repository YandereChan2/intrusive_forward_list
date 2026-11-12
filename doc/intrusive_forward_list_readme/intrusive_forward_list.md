# Yc::intrusive_forward_list

在文件 [`"intrusive_forward_list.h"`](../../intrusive_forward_list.h) 中定义。提供一套侵入式单向链表设施：

```C++
template<class vptr = void*>
class basic_intrusive_forward_list;

template<class vptr = void*>
class basic_intrusive_forward_list_iterator;

template<class vptr = void*>
class basic_intrusive_forward_list_const_iterator;

template<class vptr = void*>
class intrusive_forward_list_node_base;

template<class vptr, class T>
class intrusive_forward_list_node;

template<class vptr = void*>
using intrusive_forward_list = basic_intrusive_forward_list<vptr>;
```

_链表_ 不拥有任何 _节点_：节点的创建、析构由用户负责，链表只维护链接。
每个 _节点_ 自带一个 `next`，因此插入、摘除、拼接、合并、逆序都只是改指针，
不分配内存、不搬移元素，节点地址在链表操作前后保持不变。

`intrusive_forward_list<vptr>` 是 `basic_intrusive_forward_list<vptr>` 的便捷别名。

> 本头文件只提供结构性的链表设施，不提供拥有元素的 `std::forward_list`
> 风格容器。需要构造 / 析构 `T` 时，可以像
> [`intrusive_list_node`](https://github.com/YandereChan2/intrusive_list/blob/main/doc/intrusive_list_readme/node.md) 那样使用
> [`intrusive_forward_list_node`](#成员类型) 的 `construct_value` /
> `destroy_value`，或者直接在用户自己的节点类型里管理数据。

## 成员类型

`basic_intrusive_forward_list<vptr>` 的成员类型：

|成员类型|定义|
|-:|:-|
|`node_type`|`intrusive_forward_list_node_base<vptr>`；用户类型继承它|
|`node_ptr`|`pointer_traits<vptr>::rebind<node_type>`，节点指针（可能是缀饰指针）|
|`const_node_ptr`|`pointer_traits<vptr>::rebind<const node_type>`|
|`iterator`|可修改的前向迭代器，解引用得到 `node_type&`|
|`const_iterator`|只读的前向迭代器，解引用得到 `const node_type&`|
|`value_type`|`node_type`|
|`size_type`|`std::size_t`|
|`difference_type`|`std::ptrdiff_t`|

另外两个节点类型：

|类型|定义|
|-:|:-|
|`intrusive_forward_list_node_base<vptr>`|所有 _节点_ 的公共基类，内部维护 `next`，公开 `is_released`|
|`intrusive_forward_list_node<vptr, T>`|在前者基础上就地存放一个 `T`（`construct_value` / `destroy_value`）|

## 核心模型：纳管与脱管

每个 _节点_ 只有两种状态：

- **脱管**：默认状态。`next` 指向自身，形成自环，`node.is_released()` 为 `true`。
- **纳管**：已经被插入某条链。`next` 指向后继节点；整条链的**最后一个节点**
  的 `next` 是 `nullptr`。

```
纳管：   before_begin ──> [a] ──> [b] ──> nullptr   （end()）
脱管：   [x] ──> x
```

因此：

- `next == this` 表示脱管；
- `next == nullptr` 只出现在纳管链的尾部（或空链的 `before-begin` 锚点）；
- 两者可以明确区分，不需要额外的 `bool` 标志。

链表内部有一个 `before_begin` 锚点节点。它不是元素，`before_begin()` 返回指向它的
迭代器；它的 `next` 是首节点（空链时为 `nullptr`）。`end()` 用空指针表示，所以
`++last_node == end()`。解引用 `before_begin()` 或 `end()` 是未定义行为。

### 节点不能自己脱管

`intrusive_forward_list_node_base` **没有公开的 `release()`**。单个节点的脱管只能由
链表通过 [`erase_after`](#erase_after) 完成：`erase_after(pos)` 让 `pos` 后面的那个
节点脱管。

`clear()` 和析构函数是批量脱管，但实现上也是反复对 `before_begin()` 调用
`erase_after`，所以单点脱管仍然只有 `erase_after` 一条路径。

## 迭代器与访问

| | |
|:-|:-|
|`before_begin` / `cbefore_begin`|首元素之前的位置（锚点，不可解引用）|
|`begin` / `cbegin`|首元素；空链时等于 `end()`|
|`end` / `cend`|尾后位置（空指针）|
|`front`|首元素（空链时未定义行为）|
|`empty`|是否为空链|

`iterator` 可以隐式转换为 `const_iterator`；两者都是 `std::forward_iterator`。

## 插入

```C++
iterator insert_after(iterator pos, node_type& node) noexcept;
iterator emplace_after(iterator pos, node_type& node) noexcept;
void     push_front(node_type& node) noexcept;
node_type& emplace_front(node_type& node) noexcept;
```

在 `pos` 之后挂接 `node`，或在表头挂接 `node`。`insert_after` / `emplace_after`
返回指向 `node` 的迭代器；`emplace_front` 返回 `node` 的引用。`emplace_*` 只是
为对齐 `std::forward_list` 的名字而保留，参数仍然是节点，不构造元素。

### 前置条件（重要）

- **`node.is_released()` 必须为 `true`**。插入一个仍被任何链表纳管的节点是
  **未定义行为**。本实现**不检查**这一点，也不会先替 `node` 脱管；它会直接覆盖
  `node.next`，从而破坏原来的链（甚至形成环）。
- `pos` 必须是 `*this` 中的有效位置：`before_begin()` 或某个元素；不能是 `end()`。
- 节点一旦插入就处于纳管状态；在它仍然纳管时销毁它是未定义行为。

### 说明

- 与 `std::forward_list::insert_after` / `emplace_after` 不同（尽管本类保留了同名
  接口）：这里不构造元素，只挂接一个已经存在、已经脱管的节点。需要就地构造 `T`
  时，先构造节点，再用 `insert_after` 挂接。
- 常数时间；不分配内存、不搬移元素。
- `insert_after` 不会检查 `node` 是否已在链上，因此不要用它做"移动节点"；
  移动已纳管的节点请用 [`splice_after`](#splice_after)。
- `push_front(node)` 等价于 `insert_after(before_begin(), node)`。

## 删除与脱管

### `erase_after`

```C++
iterator erase_after(iterator pos) noexcept;
template<class Deleter>
iterator erase_after(iterator pos, Deleter del);

iterator erase_after(iterator first, iterator last) noexcept;
template<class Deleter>
iterator erase_after(iterator first, iterator last, Deleter del);
```

- 单点版本让 `pos` 后面的那个节点脱管，返回它原来的后继（脱管的是尾节点时返回
  `end()`）。`pos` 本身不受影响。
- 范围版本让 `(first, last)` 之间的节点全部脱管，返回 `last`。注意是**开区间**：
  `first` 和 `last` 本身都不脱管。因此 `erase_after(before_begin(), last)` 会从首节点
  开始脱管，而 `erase_after(begin(), end())` 会保留首节点、脱管其余全部节点。
- 无删除器版本只改链接：节点仍然存活，之后可以重新插入。
- 带删除器版本在节点已经脱管之后调用 `del(node)`，由 `del` 负责销毁 / 释放节点。
  `del` 接收 `node_type&`，不能把该节点重新挂回链表（否则会破坏删除协议）。
- 单点版本中 `pos` 必须是有效位置；对尾节点调用是未定义行为（实现防御性地返回
  `end()`）。范围版本要求 `last` 从 `first` 可达。
- 常数时间（单点）/ 与范围长度成线性（范围）；不分配内存。

### `pop_front`

```C++
void pop_front() noexcept;
template<class Deleter>
void pop_front(Deleter del);
```

脱管首节点；带删除器版本先脱管再删除。空链时是未定义行为（与 `std::forward_list`
一致）。

### `clear`

```C++
void clear() noexcept;
template<class Deleter>
void clear(Deleter del);
```

让链上所有节点脱管；带删除器版本对每个节点先脱管再调用 `del`。链表变为空链。

- `clear()` 不销毁节点、不调用删除器；之后节点可以重新插入。
- `clear(del)` 中如果 `del` 抛异常，已经处理的节点已脱管并交给 `del`，剩余节点仍
  留在链上，链表保持有效。

### 删除协议与异常

对每一个被删除的节点：

1. 先通过 `erase_after` 脱管（`next` 自指）；
2. 再调用 `del(node)`；
3. 由 `del` 负责销毁 / 释放这个节点。

因此 `del` 里可以安全地转换回用户类型并销毁对象。如果 `del` 抛异常，该节点已经
脱管，链表仍然有效；异常继续向外传播。`pred` / 比较器抛异常时，链表同样保持完整。

## 拼接

```C++
void splice_after(iterator pos, basic_intrusive_forward_list& other);
void splice_after(iterator pos, basic_intrusive_forward_list&& other);

void splice_after(iterator pos, basic_intrusive_forward_list& other, iterator it);
void splice_after(iterator pos, basic_intrusive_forward_list&& other, iterator it);

void splice_after(iterator pos, basic_intrusive_forward_list& other, iterator first, iterator last);
void splice_after(iterator pos, basic_intrusive_forward_list&& other, iterator first, iterator last);
```

- 整链版本把 `other` 的所有节点搬到 `pos` 之后，`other` 变空。
- 单点版本搬移 `other` 中 `it` **后面的那个节点**；`it` 本身留在 `other`
  （与 `std::forward_list::splice_after` 一致）。
- 范围版本搬移 `other` 中 `(first, last)` 之间的节点；`first` 和 `last` 留在 `other`。
- 被搬移的节点始终处于纳管状态，地址和迭代器不变；不会先脱管再挂接。
- `pos` 必须属于 `*this`，且不能位于被搬移的范围内。违反这些前置条件是未定义行为；
  实现会对部分自拼接情形做防御性无操作，但不要依赖它。
- 链表不保存尾指针，所以整链版本在 `pos` 不是尾节点时，需要遍历 `other` 找到其末尾
  再接到 `pos` 原来的后继上；复杂度为 `O(m)`（`m` 为 `other` 的大小）。`pos` 是尾节点
  （包括空链的 `before_begin()`）时是 `O(1)`。

## 裁剪与替换尾段

这是本类相对 `std::forward_list` 的扩展。

### `cut_after`

```C++
basic_intrusive_forward_list cut_after(iterator pos) noexcept;
```

把 `pos` 后面的所有节点从 `*this` 拆下来，装进返回的链表；`pos` 成为 `*this` 的
尾节点（`pos` 是 `before_begin()` 时 `*this` 变空）。被拆下的节点仍然**纳管**，
只是改由返回的链表持有；不销毁节点、不调用删除器。

- `pos` 必须是 `*this` 的有效位置；`pos == end()` 是未定义行为。
- 返回链表保持原有顺序，尾节点的 `next` 仍为 `nullptr`。
- 常数时间；不分配内存。
- 如果丢弃返回值，返回的临时链表会析构，被拆下的节点会回到脱管状态（不会销毁）。

### `replace_tail_after`

```C++
basic_intrusive_forward_list replace_tail_after(iterator pos, basic_intrusive_forward_list& other) noexcept;
basic_intrusive_forward_list replace_tail_after(iterator pos, basic_intrusive_forward_list&& other) noexcept;
```

等价于先 `auto tail = cut_after(pos);` 再 `splice_after(pos, other);`，最后返回
`tail`：`pos` 后面的旧节点进入返回值，`other` 的所有节点接到 `pos` 后面，`other`
变为空链。

- `pos == before_begin()` 时，`*this` 的整条链被换掉；
- `pos` 是尾节点时，返回空链，`other` 直接接到尾部；
- `other` 为空时，只做裁剪；
- `other` 必须是与 `*this` 不同的链表。`other == *this` 是未定义行为（实现不会
  形成环，但也不会把 `*this` 的前缀接到自己后面）。
- 常数时间；不分配内存。

```C++
using list = Yc::basic_intrusive_forward_list<>;

list l;          // 假设已有 [1, 2, 3, 4]
list other;      // 假设已有 [7, 8]
auto pos = l.begin();
++pos;           // 指向 2

auto cut = l.replace_tail_after(pos, other);
// l     == [1, 2, 7, 8]
// cut   == [3, 4]
// other 为空
```

## 操作

### `remove_if` / `unique`

```C++
template<class Pred, class Deleter>
size_type remove_if(Pred pred, Deleter del);
template<class Pred>
size_type remove_if(Pred pred);

template<class BinaryPred, class Deleter>
size_type unique(BinaryPred pred, Deleter del);
template<class BinaryPred>
size_type unique(BinaryPred pred);
```

- `remove_if` 删除所有使 `pred(node)` 为真的节点；`unique` 对相邻两个节点，若
  `pred(前一个, 后一个)` 为真，删除后一个（"连续相等"的一段只保留第一个）。
- 带删除器版本先脱管再调用 `del`；无删除器版本只脱管。
- 返回实际删除 / 脱管的节点数量。
- `pred` 接收 `const node_type&`；`del` 接收脱管后的 `node_type&`。

### `merge`

```C++
template<class Compare>
void merge(basic_intrusive_forward_list& other, Compare comp);
template<class Compare>
void merge(basic_intrusive_forward_list&& other, Compare comp);
```

稳定合并两条有序链；`other` 变空。相等时先保留 `*this` 中的节点，因此结果稳定。
比较器接收两个 `const node_type&`。

### `sort`

```C++
template<class Compare>
void sort(Compare comp);
```

稳定归并排序，`O(n log n)`，不分配内存。比较器接收两个 `const node_type&`。
比较器抛异常时，所有节点都会被拼回 `*this`（顺序不保证），不会丢失节点。

### `reverse`

```C++
void reverse() noexcept;
```

整条链逆序。只改指针，`O(n)`、不分配、不搬元素；节点地址不变，迭代器仍然有效
（只是它指向的节点换了位置）。

### `swap`

```C++
void swap(basic_intrusive_forward_list& other) noexcept;
friend void swap(basic_intrusive_forward_list& a, basic_intrusive_forward_list& b) noexcept;
```

交换两条链的内容，常数时间。

## 生命周期与析构

- 每个节点默认处于**脱管**状态：`next` 指向自身。
- 插入后节点处于**纳管**状态；删除后回到脱管状态。
- **析构链表对象会让整个链表的节点都脱管**（`next` 自指），但**不会销毁任何节点**，
  也不调用删除器。之后节点仍然存活，可以重新插入别的链，或者由用户销毁。
- 反过来，**在节点仍然纳管时销毁节点是未定义行为**：链上的前驱仍指向它。销毁节点
  前应先通过 `erase_after` / `clear` 让它脱管，或先让链表对象析构。
- 链表不可复制；**可移动**。移动构造 / 移动赋值只转移首节点链接，节点保持纳管，
  源链表变为空链；移动赋值会先让目标原有的节点脱管（不销毁）。节点本身不可复制。
- 链表对象只持有 `before_begin` 锚点（一个 `next` 链接），**不保存尾指针**。节点从不
  指向锚点，所以链表析构后节点之间不会留下指向已销毁对象的链接。

## 接口对齐与差异

`basic_intrusive_forward_list` 的接口按 `std::forward_list` 的形状设计，但凡涉及
"构造元素"的地方改成"插入脱管节点"，涉及"删除元素"的地方改成"脱管 + 可选删除器"。

|`std::forward_list`|本类|
|-:|:-|
|`insert_after(pos, value)` / `emplace_after(pos, args...)`|`insert_after(pos, node&)` / `emplace_after(pos, node&)`（不构造元素）|
|`push_front(value)` / `emplace_front(args...)`|`push_front(node&)` / `emplace_front(node&)`（不构造元素）|
|`erase_after(pos)`|`erase_after(pos)` 只脱管；`erase_after(pos, del)` 删除|
|`erase_after(first, last)`|同形；`(first, last)` 开区间|
|`pop_front()`|`pop_front()` 只脱管；`pop_front(del)` 删除|
|`clear()`|`clear()` 只脱管；`clear(del)` 删除|
|`remove(value)`|不提供（节点类型没有统一的"值"概念）；用 `remove_if`|
|`remove_if(pred)`|`remove_if(pred)` 只脱管；`remove_if(pred, del)` 删除|
|`unique(pred)`|`unique(pred)` 只脱管；`unique(pred, del)` 删除|
|`merge(other, comp)`|同形（节点比较器）|
|`sort(comp)`|同形（节点比较器）|
|`reverse()`|同形|
|`splice_after(...)`|同形（节点级）|
|`cut_after(pos)` / `replace_tail_after(pos, other)`|本类扩展：把尾段拆到返回链表 / 用 `other` 替换尾段|
|`front` / `empty` / 迭代器|同形|
|`assign` / `resize` / 带 count/range 的构造 / `insert_after(count, value)`|不提供：侵入式链表不拥有元素，不能凭空构造|
|`max_size` / `get_allocator`|不提供：链表不分配内存|

`std::forward_list` 的按值/按数量/按范围插入重载没有对应物：同一个节点不能重复插入，
而范围插入需要复制元素。要搬移一段已有的节点，请用 `splice_after`。

`cut_after` / `replace_tail_after` 是本类相对 `std::forward_list` 的扩展：前者把尾段
拆成一条新链表，后者在拆尾段的同时用另一条链表补上。

## 复杂度

|操作|复杂度|
|-:|:-|
|`insert_after` / `push_front` / `erase_after`（单点）/ `pop_front` / `front` / `empty` / 迭代器|常数|
|`erase_after`（范围）/ `splice_after`（范围）|与范围长度成线性|
|`splice_after`（整链）|`O(m)`；`pos` 是尾节点时为常数（`m` 为 `other` 大小）|
|`cut_after` / `replace_tail_after` / `swap`|常数|
|移动构造|常数|
|移动赋值|`O(n)`（先脱管目标原有节点）|
|`clear` / `reverse` / `remove_if` / `unique`|`O(n)`|
|`merge`|`O(n + m)`|
|`sort`|`O(n log n)`，不分配内存|

## 异常

- 插入 / 单点脱管 / 拼接 / `cut_after` / `replace_tail_after` / 逆序 / `swap` /
  移动只改指针，不分配；这些操作标记为 `noexcept`（因此缀饰指针的赋值也应当不抛异常）。
- `pred` / 比较器抛异常：链表保持完整，节点不会丢失；`sort` 会把局部链表中的
  节点全部拼回 `*this`。
- 删除器抛异常：被处理的节点已经脱管，链表仍然有效；剩余节点按当前操作的状态
  留在链上。

## 缀饰指针

模板参数 `vptr` 指定"void 指针"类型。默认 `void*`；使用缀饰指针的分配器可以传
对应的 `fancy<void>`。链表、节点、迭代器都通过
`std::pointer_traits<vptr>::rebind<...>` 取得节点指针，并用
`std::pointer_traits<...>::pointer_to` 取指针；不要求缀饰指针提供
`fancy<T>` 到 `fancy<const T>` 的转换构造函数。

例如，下面的缀饰指针故意不提供 `T -> const T` 的转换构造函数，链表仍然可以正常
进行 const 遍历：

```C++
#include <cstddef>
#include <memory>
#include <type_traits>

template<class T>
class fancy_ptr
{
    T* p_ = nullptr;
public:
    using element_type = T;
    using difference_type = std::ptrdiff_t;
    template<class U> using rebind = fancy_ptr<U>;

    fancy_ptr() = default;
    explicit fancy_ptr(T* p) : p_(p) {}

    template<class U = T, class = std::enable_if_t<!std::is_void_v<U>>>
    U& operator*() const { return *p_; }
    T* operator->() const { return p_; }
    explicit operator bool() const { return p_ != nullptr; }
    fancy_ptr& operator=(std::nullptr_t) { p_ = nullptr; return *this; }

    friend bool operator==(const fancy_ptr&, const fancy_ptr&) = default;

    template<class U = T, class = std::enable_if_t<!std::is_void_v<U>>>
    static fancy_ptr pointer_to(U& r) { return fancy_ptr{ std::addressof(r) }; }
};

using list = Yc::basic_intrusive_forward_list<fancy_ptr<void>>;
```

`test_intrusive_forward_list.cpp` 里的 `fancy_ptr` 还覆盖了 `void` 特化和
`const` 节点的 `pointer_to`，用于验证整套接口。

## 示例

```C++
#include "intrusive_forward_list.h"
#include <cassert>

struct task : Yc::intrusive_forward_list_node_base<>
{
    int id;
    explicit task(int i) : id(i) {}
};

int main()
{
    using list = Yc::basic_intrusive_forward_list<>;

    task a{ 1 }, b{ 2 }, c{ 3 };
    list l;

    l.push_front(a);                 // [1]
    l.push_front(b);                 // [2, 1]
    l.insert_after(l.begin(), c);    // [2, 3, 1]

    assert(static_cast<task&>(l.front()).id == 2);

    // 摘掉 3：只脱管，不销毁
    auto it = l.begin();             // 2
    l.erase_after(it);
    assert(c.is_released());
    assert(!b.is_released());

    // 脱管后的节点可以重新插入
    l.insert_after(l.begin(), c);    // [2, 3, 1]

    l.clear();                       // 全部脱管，a / b / c 仍然存活
    assert(a.is_released() && b.is_released() && c.is_released());
}
```

带删除器的接口用法相同，只是节点必须由与删除器匹配的方式分配。例如对 `new`
出来的节点：

```C++
using list = Yc::basic_intrusive_forward_list<>;
list l;
task* p = new task{ 1 };
l.push_front(*p);
l.erase_after(l.before_begin(), [](list::node_type& n)
    {
        delete static_cast<task*>(&n);
    });
```

## 自检

[`test_intrusive_forward_list.cpp`](../../test_intrusive_forward_list.cpp) 覆盖了：

- 纳管 / 脱管状态、`next` 自环、`is_released`；
- `insert_after` / `push_front`、`erase_after` 单点 / 范围、`pop_front`、`clear`；
- 删除器协议、`remove_if` / `unique` 有无删除器；
- `splice_after` 整链 / 单点 / 范围 / 自拼接；
- `cut_after` 在表头 / 中间 / 尾节点 / 空链上的裁剪；
- `replace_tail_after` 的左值 / 右值 `other`、空 `other`、表头 / 尾节点位置；
- 移动构造 / 移动赋值，以及移动后源链表为空、目标原有节点脱管；
- `merge` 稳定性、`sort` 稳定性与比较器抛异常、`reverse` 的迭代器稳定性；
- 链表析构后节点脱管；
- 用不提供 `T -> const T` 转换的缀饰指针跑通全部主要接口。

# Yc::intrusive_forward_list

侵入式单向链表，接口按 `std::forward_list` 的形状设计，但不拥有元素：
链表只维护 `next` 链接，节点的创建、析构由用户负责。

- 节点区分**纳管**与**脱管**：脱管节点的 `next` 指向自身形成自环；
  纳管链的尾节点 `next` 为 `nullptr`。
- 节点没有公开的 `release()`。单点脱管只能由链表通过 `erase_after` 完成；
  `clear()` 和析构函数也是反复对 `before_begin()` 调用 `erase_after`。
- 插入接口要求传入**已脱管**节点；插入仍纳管的节点是未定义行为（不检查）。
- 删除接口提供“只脱管”和“脱管 + 调用删除器”两种形式。
- 支持缀饰指针（fancy pointer）：通过 `pointer_traits<vptr>::rebind` 使用节点指针。
- 扩展：`cut_after(pos)` / `replace_tail_after(pos, other)` 用来拆下尾段、
  以及用另一条链替换尾段。
- header-only，要求 C++20 或更高。

## 文件

| 文件 | 说明 |
|---|---|
| [`intrusive_forward_list.h`](intrusive_forward_list.h) | 实现 |
| [`test_intrusive_forward_list.cpp`](test_intrusive_forward_list.cpp) | 自测：状态、删除器、排序/合并、拼接、裁剪、移动、缀饰指针 |
| [`CMakeLists.txt`](CMakeLists.txt) | 构建与测试 |
| [`doc/intrusive_forward_list_readme/`](doc/intrusive_forward_list_readme/intrusive_forward_list.md) | 完整文档 |

## 快速开始

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

    // 把 begin() 后面的节点拆成一条新链
    auto tail = l.cut_after(l.begin());
    assert(static_cast<task&>(tail.front()).id == 3);

    l.clear();                       // 只脱管，不销毁
    tail.clear();
    assert(a.is_released() && b.is_released() && c.is_released());
}
```

## 构建测试

```bat
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## 文档

完整接口、生命周期、复杂度、未定义行为与设计说明见
[`doc/intrusive_forward_list_readme/intrusive_forward_list.md`](doc/intrusive_forward_list_readme/intrusive_forward_list.md)。

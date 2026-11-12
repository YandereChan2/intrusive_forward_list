#pragma once
#include <cstddef>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>

namespace Yc
{
    template<class vptr = void*>
    class basic_intrusive_forward_list;
    template<class vptr = void*>
    class basic_intrusive_forward_list_iterator;
    template<class vptr = void*>
    class basic_intrusive_forward_list_const_iterator;

    // ===================================================================
    // 单向侵入式链表节点基类。
    //
    // 每个节点只有一个 next 链接，节点有两种状态：
    //
    //   纳管：next 指向后继节点；链尾节点的 next 是 nullptr。
    //   脱管：next 指向自己，形成自环。默认构造的节点就是脱管状态。
    //
    // 因此 "next == this" 是脱管标记，而 "next == nullptr" 只可能出现在
    // 纳管链的尾部。节点没有公开的 release()：单个节点的脱管只能由链表
    // 通过 erase_after 完成（clear / 析构是批量地反复调用 erase_after）。
    // ===================================================================
    template<class vptr = void*>
    class intrusive_forward_list_node_base
    {
    public:
        // Fancy pointer obtained from the template parameter vptr (a void pointer type)
        // through pointer_traits: its element type is this node type, not void.
        using node_pointer = typename std::pointer_traits<vptr>::template rebind<intrusive_forward_list_node_base>;
        static_assert(std::is_same_v<typename std::pointer_traits<node_pointer>::element_type, intrusive_forward_list_node_base>,
            "pointer_traits<vptr>::rebind must yield a pointer whose element_type is intrusive_forward_list_node_base");
        static_assert(std::is_same_v<node_pointer, typename std::pointer_traits<node_pointer>::template rebind<intrusive_forward_list_node_base>>,
            "rebinding node_pointer to the node type must yield node_pointer itself");
    private:
        node_pointer next{ to_node_ptr(*this) };

        static node_pointer to_node_ptr(intrusive_forward_list_node_base& node)noexcept
        {
            return std::pointer_traits<node_pointer>::pointer_to(node);
        }
        // 仅链表（以及由链表调用的 erase_after）可以让节点脱管。
        void detach()noexcept
        {
            next = to_node_ptr(*this);
        }
    public:
        intrusive_forward_list_node_base() = default;
        // 链接是自指的（或指向别的节点），逐位复制会让副本的链接仍指向
        // 源对象，从而使链表结构静默损坏，因此禁止拷贝。
        // 节点本来就是就地构造、挂在链表里的对象，不支持搬迁语义。
        intrusive_forward_list_node_base(const intrusive_forward_list_node_base&) = delete;
        intrusive_forward_list_node_base& operator=(const intrusive_forward_list_node_base&) = delete;
        // 是否处于脱管状态（next 指向自己）。!is_released() 即纳管。
        bool is_released()const noexcept
        {
            return next == to_node_ptr(const_cast<intrusive_forward_list_node_base&>(*this));
        }
        // 注意：这里的 friend 声明不能再写一遍 template<class vptr = void*>，
        // 那会把外层的 vptr 遮蔽掉（GCC 会直接报错）。
        template<class>
        friend class basic_intrusive_forward_list;
        template<class>
        friend class basic_intrusive_forward_list_iterator;
        template<class>
        friend class basic_intrusive_forward_list_const_iterator;
    };

    // 前向迭代器。end() 用空指针表示；before_begin() 指向链表内部的
    // before-begin 锚点（不是元素，解引用是未定义行为）。
    template<class vptr>
    class basic_intrusive_forward_list_iterator
    {
    public:
        using node_type = intrusive_forward_list_node_base<vptr>;
        using node_ptr = typename std::pointer_traits<vptr>::template rebind<node_type>;
        using const_node_ptr = typename std::pointer_traits<vptr>::template rebind<const node_type>;
        using iterator_category = std::forward_iterator_tag;
        using iterator_concept = std::forward_iterator_tag;
        using value_type = node_type;
        using difference_type = std::ptrdiff_t;
        using pointer = node_type*;
        using reference = node_type&;
    private:
        node_ptr p{};
    public:
        basic_intrusive_forward_list_iterator() = default;
        explicit basic_intrusive_forward_list_iterator(node_ptr ptr)noexcept : p(ptr) {}
        auto base()const noexcept
        {
            return p;
        }
        // 迭代器指向的节点（basic_intrusive_forward_list 的节点类型即 node_type）
        node_type& operator*()const noexcept
        {
            return *p;
        }
        node_type* operator->()const noexcept
        {
            return std::addressof(*p);
        }
        basic_intrusive_forward_list_iterator& operator++()noexcept
        {
            p = p->next;
            return *this;
        }
        basic_intrusive_forward_list_iterator operator++(int)noexcept
        {
            auto tmp = *this;
            ++*this;
            return tmp;
        }
        friend bool operator==(basic_intrusive_forward_list_iterator, basic_intrusive_forward_list_iterator)noexcept = default;
        // 允许 iterator -> const_iterator。
        // 这里用 pointer_to 从节点本身重新取 const 指针，而不是要求
        // fancy pointer 提供 T -> const T 的转换构造函数。
        operator basic_intrusive_forward_list_const_iterator<vptr>()const noexcept
        {
            if (p == node_ptr{})
            {
                return basic_intrusive_forward_list_const_iterator<vptr>{};
            }
            return basic_intrusive_forward_list_const_iterator<vptr>{
                std::pointer_traits<const_node_ptr>::pointer_to(*p) };
        }
        template<class>
        friend class basic_intrusive_forward_list;
    };

    // 常量迭代器：与 iterator 遍历逻辑相同，解引用返回 const 节点。
    template<class vptr>
    class basic_intrusive_forward_list_const_iterator
    {
    public:
        using node_type = intrusive_forward_list_node_base<vptr>;
        using node_ptr = typename std::pointer_traits<vptr>::template rebind<node_type>;
        using const_node_ptr = typename std::pointer_traits<vptr>::template rebind<const node_type>;
        using iterator_category = std::forward_iterator_tag;
        using iterator_concept = std::forward_iterator_tag;
        using value_type = node_type;
        using difference_type = std::ptrdiff_t;
        using pointer = const node_type*;
        using reference = const node_type&;
    private:
        const_node_ptr p{};

        // 从 const 节点走 next 时，next 字段本身是 node_ptr（非 const）；
        // 同样用 pointer_to 重新取 const 指针，避免依赖 fancy pointer 的
        // T -> const T 转换构造函数。
        static const_node_ptr next_of(const_node_ptr ptr)noexcept
        {
            if (ptr == const_node_ptr{})
            {
                return const_node_ptr{};
            }
            node_ptr n = ptr->next;
            if (n == node_ptr{})
            {
                return const_node_ptr{};
            }
            return std::pointer_traits<const_node_ptr>::pointer_to(*n);
        }
    public:
        basic_intrusive_forward_list_const_iterator() = default;
        explicit basic_intrusive_forward_list_const_iterator(const_node_ptr ptr)noexcept : p(ptr) {}
        auto base()const noexcept
        {
            return p;
        }
        const node_type& operator*()const noexcept
        {
            return *p;
        }
        const node_type* operator->()const noexcept
        {
            return std::addressof(*p);
        }
        basic_intrusive_forward_list_const_iterator& operator++()noexcept
        {
            p = next_of(p);
            return *this;
        }
        basic_intrusive_forward_list_const_iterator operator++(int)noexcept
        {
            auto tmp = *this;
            ++*this;
            return tmp;
        }
        friend bool operator==(basic_intrusive_forward_list_const_iterator, basic_intrusive_forward_list_const_iterator)noexcept = default;
        template<class>
        friend class basic_intrusive_forward_list;
        template<class>
        friend class basic_intrusive_forward_list_iterator;
    };

    // ===================================================================
    // 单向侵入式链表。
    //
    // 链表不拥有节点，只维护链接。链表内部有一个 before-begin 锚点：
    //   before_begin().base()->next 是首节点（空链为 nullptr）；
    //   end() 是空指针。
    // 节点本身从不指向这个锚点，所以析构链表时只要把节点逐个脱管即可，
    // 不会留下指向链表对象的悬空链接。链表对象只持有这个锚点，不保存尾指针。
    //
    // 插入接口（insert_after / push_front）要求传入的节点已经脱管
    //（is_released() == true）。插入一个仍被任何链表纳管的节点是
    // 未定义行为；本实现不做检查。
    //
    // 删除接口把节点脱管后交给删除器（deleter 版本），或者只脱管
    //（无删除器版本）。erase_after 是唯一的单点脱管原语；clear 和
    // 析构函数都是反复对 before_begin 调用 erase_after。
    //
    // 链表不可复制；可移动（移动只转移首节点链接，源链表变为空链）。
    // ===================================================================
    template<class vptr>
    class basic_intrusive_forward_list
    {
    public:
        using node_type = intrusive_forward_list_node_base<vptr>;
        using node_ptr = typename std::pointer_traits<vptr>::template rebind<node_type>;
        using const_node_ptr = typename std::pointer_traits<vptr>::template rebind<const node_type>;
        using iterator = basic_intrusive_forward_list_iterator<vptr>;
        using const_iterator = basic_intrusive_forward_list_const_iterator<vptr>;
        using value_type = node_type;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using reference = node_type&;
        using const_reference = const node_type&;
    private:
        node_type before_begin_{};

        static node_ptr to_node_ptr(node_type& node)noexcept
        {
            return std::pointer_traits<node_ptr>::pointer_to(node);
        }
        static const_node_ptr to_const_node_ptr(const node_type& node)noexcept
        {
            return std::pointer_traits<const_node_ptr>::pointer_to(const_cast<node_type&>(node));
        }
        // node_ptr -> const_node_ptr，不要求 fancy pointer 支持
        // T -> const T 的转换构造函数。
        static const_node_ptr to_const_ptr(node_ptr p)noexcept
        {
            if (p == node_ptr{})
            {
                return const_node_ptr{};
            }
            return std::pointer_traits<const_node_ptr>::pointer_to(*p);
        }
    public:
        basic_intrusive_forward_list()noexcept
        {
            before_begin_.next = node_ptr{};
        }
        // 析构链表对象：让链上所有节点脱管（next 自指），不销毁任何节点。
        ~basic_intrusive_forward_list()
        {
            clear();
        }
        basic_intrusive_forward_list(const basic_intrusive_forward_list&) = delete;
        basic_intrusive_forward_list& operator=(const basic_intrusive_forward_list&) = delete;
        // 移动只转移首节点链接；节点仍然纳管，源链表变为空链。
        basic_intrusive_forward_list(basic_intrusive_forward_list&& other)noexcept
            : basic_intrusive_forward_list()
        {
            before_begin_.next = other.before_begin_.next;
            other.before_begin_.next = node_ptr{};
        }
        // 移动赋值先让目标原有的节点脱管（不销毁），再接管源链表的节点。
        basic_intrusive_forward_list& operator=(basic_intrusive_forward_list&& other)noexcept
        {
            if (this != &other)
            {
                clear();
                before_begin_.next = other.before_begin_.next;
                other.before_begin_.next = node_ptr{};
            }
            return *this;
        }

        void swap(basic_intrusive_forward_list& other)noexcept
        {
            node_ptr head = before_begin_.next;
            before_begin_.next = other.before_begin_.next;
            other.before_begin_.next = head;
        }
        friend void swap(basic_intrusive_forward_list& a, basic_intrusive_forward_list& b)noexcept
        {
            a.swap(b);
        }

        // ---------------- 迭代器 ----------------
        iterator before_begin()noexcept
        {
            return iterator{ to_node_ptr(before_begin_) };
        }
        const_iterator before_begin()const noexcept
        {
            return const_iterator{ to_const_node_ptr(before_begin_) };
        }
        const_iterator cbefore_begin()const noexcept
        {
            return const_iterator{ to_const_node_ptr(before_begin_) };
        }
        iterator begin()noexcept
        {
            return iterator{ before_begin_.next };
        }
        const_iterator begin()const noexcept
        {
            return const_iterator{ to_const_ptr(before_begin_.next) };
        }
        const_iterator cbegin()const noexcept
        {
            return begin();
        }
        iterator end()noexcept
        {
            return iterator{};
        }
        const_iterator end()const noexcept
        {
            return const_iterator{};
        }
        const_iterator cend()const noexcept
        {
            return end();
        }

        // ---------------- 访问 ----------------
        bool empty()const noexcept
        {
            return before_begin_.next == node_ptr{};
        }
        node_type& front()noexcept
        {
            return *before_begin_.next;
        }
        const node_type& front()const noexcept
        {
            return *to_const_ptr(before_begin_.next);
        }

        // ---------------- 插入 ----------------
        // 在 pos 之后挂接 node，返回指向 node 的迭代器。
        //
        // 前置条件：node.is_released() == true。插入一个仍被任何链表
        // 纳管的节点是未定义行为；本函数不检查，也不会先替 node 脱管。
        // pos 必须是 *this 中的有效位置（before_begin() 或某个元素），
        // 不能是 end()。
        iterator insert_after(iterator pos, node_type& node)noexcept
        {
            node_ptr prev = pos.base();
            node_ptr next = prev->next;
            node_ptr np = to_node_ptr(node);
            node.next = next;
            prev->next = np;
            return iterator{ np };
        }
        void push_front(node_type& node)noexcept
        {
            insert_after(before_begin(), node);
        }

        // ---------------- 移除 ----------------
        // 脱管 pos 后面的节点，返回它原来的后继（脱管的是尾节点时返回 end()）。
        // 无删除器版本只改链接；节点仍然存活，之后可以重新插入。
        // pos 必须是有效位置；对尾节点调用是未定义行为（这里防御性地返回 end()）。
        iterator erase_after(iterator pos)noexcept
        {
            node_ptr prev = pos.base();
            if (prev == node_ptr{})
            {
                return end();
            }
            node_ptr target = prev->next;
            if (target == node_ptr{})
            {
                return end();
            }
            node_ptr next = target->next;
            prev->next = next;
            target->detach();
            return iterator{ next };
        }
        // 脱管 pos 后面的节点，先脱管再调用 del(node)，返回原来的后继。
        // del 接收 node_type&，负责销毁 / 释放节点；它不能把节点重新挂回链表。
        template<class Deleter>
        iterator erase_after(iterator pos, Deleter del)
        {
            node_ptr prev = pos.base();
            if (prev == node_ptr{})
            {
                return end();
            }
            node_ptr target = prev->next;
            if (target == node_ptr{})
            {
                return end();
            }
            node_ptr next = target->next;
            prev->next = next;
            target->detach();
            del(*target);
            return iterator{ next };
        }
        // 脱管 (first, last) 之间的所有节点，返回 last。
        // first 本身不脱管，last 也不脱管。范围必须可达。
        iterator erase_after(iterator first, iterator last)noexcept
        {
            node_ptr prev = first.base();
            node_ptr stop = last.base();
            if (prev == node_ptr{} || prev == stop)
            {
                return last;
            }
            while (prev->next != stop && prev->next != node_ptr{})
            {
                erase_after(iterator{ prev });
            }
            return last;
        }
        template<class Deleter>
        iterator erase_after(iterator first, iterator last, Deleter del)
        {
            node_ptr prev = first.base();
            node_ptr stop = last.base();
            if (prev == node_ptr{} || prev == stop)
            {
                return last;
            }
            while (prev->next != stop && prev->next != node_ptr{})
            {
                erase_after(iterator{ prev }, del);
            }
            return last;
        }
        void pop_front()noexcept
        {
            erase_after(before_begin());
        }
        template<class Deleter>
        void pop_front(Deleter del)
        {
            erase_after(before_begin(), del);
        }

        // 让链上所有节点脱管；不销毁节点、不调用删除器。
        // 实现上反复对 before_begin 调用 erase_after，因此单点脱管仍然
        // 只有 erase_after 一条路径。
        void clear()noexcept
        {
            while (!empty())
            {
                erase_after(before_begin());
            }
        }
        // 让链上所有节点脱管，并对每个节点调用 del。异常从 del 抛出时，
        // 已经处理的节点已脱管并交给 del，剩余节点仍留在链上。
        template<class Deleter>
        void clear(Deleter del)
        {
            while (!empty())
            {
                erase_after(before_begin(), del);
            }
        }

        // ---------------- 拼接 ----------------
        // 把 other 的所有节点搬到 pos 之后；other 变为空链。节点保持纳管，
        // 地址和迭代器不变。pos 必须属于 *this，且不能位于 other 的范围内。
        void splice_after(iterator pos, basic_intrusive_forward_list& other)noexcept
        {
            if (other.empty() || &other == this)
            {
                return;
            }
            node_ptr p = pos.base();
            node_ptr first = other.before_begin_.next;
            node_ptr after = p->next;
            p->next = first;
            if (after != node_ptr{})
            {
                // 没有尾指针：pos 不是尾节点时，要走到 other 的末尾再接到 after。
                node_ptr last = first;
                while (last->next != node_ptr{})
                {
                    last = last->next;
                }
                last->next = after;
            }
            other.before_begin_.next = node_ptr{};
        }
        void splice_after(iterator pos, basic_intrusive_forward_list&& other)noexcept
        {
            splice_after(pos, other);
        }
        // 把 other 中 it 后面的那个节点搬到 pos 之后。it 本身留在 other。
        void splice_after(iterator pos, basic_intrusive_forward_list&, iterator it)noexcept
        {
            node_ptr prev = it.base();
            if (prev == node_ptr{})
            {
                return;
            }
            node_ptr moved = prev->next;
            if (moved == node_ptr{})
            {
                return;
            }
            node_ptr p = pos.base();
            if (p == node_ptr{} || p == moved)
            {
                return;
            }
            node_ptr after = moved->next;
            prev->next = after;
            node_ptr pos_next = p->next;
            p->next = moved;
            moved->next = pos_next;
        }
        void splice_after(iterator pos, basic_intrusive_forward_list&& other, iterator it)noexcept
        {
            splice_after(pos, other, it);
        }
        // 把 other 中 (first, last) 之间的节点搬到 pos 之后；last 仍然
        // 有效（last 留在 other）。first 和 last 本身不移动。
        void splice_after(iterator pos, basic_intrusive_forward_list&, iterator first, iterator last)noexcept
        {
            node_ptr f = first.base();
            node_ptr l = last.base();
            if (f == node_ptr{})
            {
                return;
            }
            node_ptr range_first = f->next;
            if (range_first == l || range_first == node_ptr{})
            {
                return;
            }
            node_ptr p = pos.base();
            if (p == node_ptr{})
            {
                return;
            }
            // 先找到 last 前面的节点；顺便防止 pos 落在被搬移的范围内
            //（标准规定这是未定义行为，这里防御性地变成无操作）。
            node_ptr range_last = range_first;
            for (;;)
            {
                if (range_last == p)
                {
                    return;
                }
                if (range_last->next == l)
                {
                    break;
                }
                if (range_last->next == node_ptr{})
                {
                    return;
                }
                range_last = range_last->next;
            }
            // 从 other 摘下 (first, last)
            f->next = l;
            // 接到 pos 后面
            node_ptr after = p->next;
            p->next = range_first;
            range_last->next = after;
        }
        void splice_after(iterator pos, basic_intrusive_forward_list&& other, iterator first, iterator last)noexcept
        {
            splice_after(pos, other, first, last);
        }

        // ---------------- 裁剪 / 替换尾段 ----------------
        // 把 pos 后面的所有节点拆下来，交给返回的链表；pos 成为 *this 的尾节点
        //（pos 是 before_begin() 时 *this 变空）。被拆下的节点仍然纳管，只是改由
        // 返回值持有；不销毁节点、不调用删除器。
        // pos 必须属于 *this；pos == end() 是未定义行为。
        basic_intrusive_forward_list cut_after(iterator pos)noexcept
        {
            basic_intrusive_forward_list tail;
            node_ptr p = pos.base();
            if (p == node_ptr{})
            {
                return tail;
            }
            tail.before_begin_.next = p->next;
            p->next = node_ptr{};
            return tail;
        }

        // 先做 cut_after(pos)，再把 other 的所有节点接到 pos 后面；other 变为空链。
        // 返回被拆下来的尾段。other 必须是与 *this 不同的链表；other == *this
        // 是未定义行为（这里不会形成环，但也不会把 *this 的前缀接到自己后面）。
        basic_intrusive_forward_list replace_tail_after(iterator pos, basic_intrusive_forward_list& other)noexcept
        {
            if (pos.base() == node_ptr{})
            {
                return basic_intrusive_forward_list{};
            }
            basic_intrusive_forward_list tail = cut_after(pos);
            splice_after(pos, other);
            return tail;
        }
        basic_intrusive_forward_list replace_tail_after(iterator pos, basic_intrusive_forward_list&& other)noexcept
        {
            return replace_tail_after(pos, other);
        }

        // ---------------- 操作 ----------------
        // 按谓词删除节点：先脱管，再交给 del。返回实际删除的数量。
        // pred 接收 const node_type&，del 接收脱管后的 node_type&。
        template<class Pred, class Deleter>
        size_type remove_if(Pred pred, Deleter del)
        {
            size_type removed = 0;
            iterator prev = before_begin();
            for (;;)
            {
                iterator cur = prev;
                ++cur;
                if (cur == end())
                {
                    break;
                }
                if (pred(*cur))
                {
                    erase_after(prev, del);
                    ++removed;
                }
                else
                {
                    prev = cur;
                }
            }
            return removed;
        }
        // 只脱管，不调用删除器；返回脱管的节点数量。
        template<class Pred>
        size_type remove_if(Pred pred)
        {
            size_type removed = 0;
            iterator prev = before_begin();
            for (;;)
            {
                iterator cur = prev;
                ++cur;
                if (cur == end())
                {
                    break;
                }
                if (pred(*cur))
                {
                    erase_after(prev);
                    ++removed;
                }
                else
                {
                    prev = cur;
                }
            }
            return removed;
        }
        // 相邻去重：pred(前一个, 后一个) 为真时删除后一个。先脱管再 del。
        template<class BinaryPred, class Deleter>
        size_type unique(BinaryPred pred, Deleter del)
        {
            if (empty())
            {
                return 0;
            }
            size_type removed = 0;
            iterator prev = begin();
            for (;;)
            {
                iterator cur = prev;
                ++cur;
                if (cur == end())
                {
                    break;
                }
                if (pred(*prev, *cur))
                {
                    erase_after(prev, del);
                    ++removed;
                }
                else
                {
                    prev = cur;
                }
            }
            return removed;
        }
        template<class BinaryPred>
        size_type unique(BinaryPred pred)
        {
            if (empty())
            {
                return 0;
            }
            size_type removed = 0;
            iterator prev = begin();
            for (;;)
            {
                iterator cur = prev;
                ++cur;
                if (cur == end())
                {
                    break;
                }
                if (pred(*prev, *cur))
                {
                    erase_after(prev);
                    ++removed;
                }
                else
                {
                    prev = cur;
                }
            }
            return removed;
        }

        // 稳定合并：把有序的 other 合并进有序的 *this；other 变空。
        // 相等时先保留 *this 中的节点，保持稳定。
        template<class Compare>
        void merge(basic_intrusive_forward_list& other, Compare comp)
        {
            if (&other == this || other.empty())
            {
                return;
            }
            node_ptr prev = to_node_ptr(before_begin_);
            node_ptr cur1 = prev->next;
            node_ptr cur2 = other.before_begin_.next;
            while (cur1 != node_ptr{} && cur2 != node_ptr{})
            {
                if (comp(*cur2, *cur1))
                {
                    node_ptr next2 = cur2->next;
                    // cur2 是 other 的首节点，从 other 摘下
                    other.before_begin_.next = next2;
                    // 插到 prev 后面
                    prev->next = cur2;
                    cur2->next = cur1;
                    prev = cur2;
                    cur2 = next2;
                }
                else
                {
                    prev = cur1;
                    cur1 = cur1->next;
                }
            }
            if (cur2 != node_ptr{})
            {
                // other 剩下的整段直接接在末尾，O(1)
                prev->next = cur2;
                other.before_begin_.next = node_ptr{};
            }
        }
        template<class Compare>
        void merge(basic_intrusive_forward_list&& other, Compare comp)
        {
            merge(other, comp);
        }

        // 稳定归并排序，O(n log n)，不分配内存。比较器接收 const node_type&。
        // 比较器抛异常时，所有节点都会被拼回 *this（顺序不保证），不会丢失。
        template<class Compare>
        void sort(Compare comp)
        {
            if (empty())
            {
                return;
            }
            auto second = begin();
            ++second;
            if (second == end())
            {
                return;
            }

            // 自底向上归并：bin[i] 保存长度 2^i 的有序段，O(log n) 个链表。
            constexpr std::size_t max_bins = 64;
            basic_intrusive_forward_list bins[max_bins];
            basic_intrusive_forward_list run;

            // 比较器抛异常时把 bins/run 里的节点全部拼回 *this，
            // 保证链表仍然完整（顺序不保证），不会把节点留在局部链表里。
            struct rollback_guard
            {
                basic_intrusive_forward_list* self;
                basic_intrusive_forward_list* bins;
                basic_intrusive_forward_list* run;
                std::size_t bin_count;
                bool active = true;
                ~rollback_guard()
                {
                    if (!active)
                    {
                        return;
                    }
                    for (std::size_t i = 0; i != bin_count; ++i)
                    {
                        if (!bins[i].empty())
                        {
                            self->splice_after(self->before_begin(), bins[i]);
                        }
                    }
                    if (!run->empty())
                    {
                        self->splice_after(self->before_begin(), *run);
                    }
                }
            } guard{ this, bins, &run, max_bins };

            while (!empty())
            {
                // 取 *this 的首节点，作为长度 1 的段放入 run
                run.splice_after(run.before_begin(), *this, before_begin());
                std::size_t i = 0;
                while (i != max_bins && !bins[i].empty())
                {
                    bins[i].merge(run, comp);            // 相等时取 bins[i]，保持稳定
                    run.splice_after(run.before_begin(), bins[i]);
                    ++i;
                }
                if (i == max_bins)
                {
                    break;                               // 2^64 个节点，实际不会到这里
                }
                bins[i].splice_after(bins[i].before_begin(), run);
            }
            if (!run.empty())
            {
                merge(run, comp);
            }

            // 高位 bin 是更早的段，从高到低合并才能保持稳定。
            for (std::size_t i = max_bins; i != 0;)
            {
                --i;
                if (!bins[i].empty())
                {
                    merge(bins[i], comp);
                }
            }
            guard.active = false;
        }

        // 整条链逆序：只改指针，O(n)、不分配、不搬元素；节点地址不变，
        // 所以迭代器仍然有效（只是它指向的节点换了位置）。
        void reverse()noexcept
        {
            if (empty())
            {
                return;
            }
            node_ptr prev{};
            node_ptr cur = before_begin_.next;
            while (cur != node_ptr{})
            {
                node_ptr next = cur->next;
                cur->next = prev;
                prev = cur;
                cur = next;
            }
            before_begin_.next = prev;
        }
    };

    // 带数据的侵入式单向链表节点：既可以被 basic_intrusive_forward_list
    // 串起来，又存放用户数据 T。数据用 union 包裹，便于由分配器分别负责
    // 构造 / 析构。
    template<class vptr, class T>
    class intrusive_forward_list_node : public intrusive_forward_list_node_base<vptr>
    {
        union data_t
        {
            T data;
            data_t() {}
            ~data_t() {}
        }d;
    public:
        // 注意：必须有这个显式默认构造。否则 allocator_traits::construct 走
        // 的是隐式默认构造，它不会调用基类 intrusive_forward_list_node_base
        // 的 NSDMI（next 保持垃圾值），挂链时就会踩野指针。
        intrusive_forward_list_node() = default;
        template<class Alloc, class... Args>
        void construct_value(Alloc& alloc, Args&&... args)noexcept(std::is_nothrow_constructible_v<T, Args...>)
        {
            std::allocator_traits<Alloc>::construct(alloc, std::addressof(d.data), std::forward<Args>(args)...);
        }
        template<class Alloc>
        void destroy_value(Alloc& alloc)noexcept
        {
            std::allocator_traits<Alloc>::destroy(alloc, std::addressof(d.data));
        }
        T& value()noexcept
        {
            return d.data;
        }
        const T& value()const noexcept
        {
            return d.data;
        }
    };

    // 便捷别名：默认 vptr 为 void*。
    template<class vptr = void*>
    using intrusive_forward_list = basic_intrusive_forward_list<vptr>;
}

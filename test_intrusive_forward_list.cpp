// 测试必须在所有配置下都启用 assert（Release 默认会定义 NDEBUG）。
#ifdef NDEBUG
#undef NDEBUG
#endif

#include "intrusive_forward_list.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

using base = Yc::intrusive_forward_list_node_base<void*>;
using list = Yc::basic_intrusive_forward_list<void*>;

static_assert(std::is_same_v<Yc::intrusive_forward_list<>, list>);
// 没有尾指针：链表只持有一个 before_begin 锚点。
static_assert(sizeof(list) == sizeof(list::node_ptr));

// 节点自己不能脱管：没有公开的 release()。
template<class T>
concept has_release = requires(T& n) { n.release(); };
static_assert(!has_release<base>);
static_assert(std::forward_iterator<list::iterator>);
static_assert(std::forward_iterator<list::const_iterator>);

struct node : Yc::intrusive_forward_list_node<void*, int>
{
    static inline int alive = 0;
    int id = 0;

    explicit node(int v, int i = 0) : id(i)
    {
        std::allocator<node> a;
        construct_value(a, v);
        ++alive;
    }

    ~node()
    {
        std::allocator<node> a;
        destroy_value(a);
        --alive;
    }
};

static const node& as_node(const base& n) noexcept
{
    return static_cast<const node&>(n);
}

static int value_of(const base& n) noexcept
{
    return as_node(n).value();
}

static auto less = [](const base& a, const base& b)
{
    return value_of(a) < value_of(b);
};

static auto even = [](const base& n)
{
    return value_of(n) % 2 == 0;
};

static auto equal = [](const base& a, const base& b)
{
    return value_of(a) == value_of(b);
};

static void delete_node(base& n)
{
    delete static_cast<node*>(&n);
}

static std::vector<int> values(const list& l)
{
    std::vector<int> out;
    for (auto it = l.begin(); it != l.end(); ++it)
    {
        out.push_back(value_of(*it));
    }
    return out;
}

static std::vector<int> ids(const list& l)
{
    std::vector<int> out;
    for (auto it = l.begin(); it != l.end(); ++it)
    {
        out.push_back(as_node(*it).id);
    }
    return out;
}

static void check_tail(const list& l)
{
    if (l.empty())
    {
        return;
    }
    auto it = l.begin();
    auto last = it;
    for (; it != l.end(); ++it)
    {
        last = it;
    }
    auto after = last;
    ++after;
    assert(after == l.end());
}

static void delete_remaining(list& l)
{
    std::vector<node*> remain;
    for (auto it = l.begin(); it != l.end(); ++it)
    {
        remain.push_back(static_cast<node*>(&*it));
    }
    l.clear();
    for (auto* p : remain)
    {
        delete p;
    }
}

// ---------------------------------------------------------------------------
// 缀饰指针（fancy pointer）：故意不提供 T -> const T 的转换构造函数，
// 以验证链表/迭代器只通过 pointer_traits::pointer_to 取 const 指针。
// ---------------------------------------------------------------------------
template<class T>
class fancy_ptr
{
    T* p_ = nullptr;
public:
    using element_type = T;
    using difference_type = std::ptrdiff_t;
    template<class U> using rebind = fancy_ptr<U>;

    constexpr fancy_ptr() noexcept = default;
    constexpr explicit fancy_ptr(T* p) noexcept : p_(p) {}

    template<class U = T, std::enable_if_t<!std::is_void_v<U>, int> = 0>
    constexpr U& operator*() const noexcept
    {
        return *p_;
    }
    constexpr T* operator->() const noexcept
    {
        return p_;
    }
    constexpr explicit operator bool() const noexcept
    {
        return p_ != nullptr;
    }
    constexpr fancy_ptr& operator=(std::nullptr_t) noexcept
    {
        p_ = nullptr;
        return *this;
    }
    friend constexpr bool operator==(const fancy_ptr&, const fancy_ptr&) noexcept = default;

    template<class U = T, std::enable_if_t<!std::is_void_v<U>, int> = 0>
    static fancy_ptr pointer_to(U& r) noexcept
    {
        return fancy_ptr{ std::addressof(r) };
    }
};

using fvptr = fancy_ptr<void>;
using fbase = Yc::intrusive_forward_list_node_base<fvptr>;
using flist = Yc::basic_intrusive_forward_list<fvptr>;

struct fnode : Yc::intrusive_forward_list_node<fvptr, int>
{
    static inline int alive = 0;
    int id = 0;

    explicit fnode(int v, int i = 0) : id(i)
    {
        std::allocator<fnode> a;
        construct_value(a, v);
        ++alive;
    }

    ~fnode()
    {
        std::allocator<fnode> a;
        destroy_value(a);
        --alive;
    }
};

static std::vector<int> fvalues(const flist& l)
{
    std::vector<int> out;
    for (auto it = l.begin(); it != l.end(); ++it)
    {
        out.push_back(static_cast<const fnode&>(*it).value());
    }
    return out;
}

static void check_tail_f(const flist& l)
{
    if (l.empty())
    {
        return;
    }
    auto it = l.begin();
    auto last = it;
    for (; it != l.end(); ++it)
    {
        last = it;
    }
    auto after = last;
    ++after;
    assert(after == l.end());
}

// ---------------------------------------------------------------------------

static void test_basic()
{
    node a(1, 1);
    assert(a.is_released());

    list l;
    assert(l.empty());
    assert(l.begin() == l.end());
    assert(l.before_begin() != l.end());
    assert(l.erase_after(l.before_begin()) == l.end());

    l.push_front(a);
    assert(!l.empty());
    assert(!a.is_released());
    assert(value_of(l.front()) == 1);
    assert(l.before_begin() != l.begin());
    assert(++l.before_begin() == l.begin());

    node b(2, 2);
    l.insert_after(l.before_begin(), b);          // [2, 1]
    assert((values(l) == std::vector<int>{2, 1}));

    node c(3, 3);
    l.insert_after(l.begin(), c);                 // [2, 3, 1]
    assert((values(l) == std::vector<int>{2, 3, 1}));
    check_tail(l);

    node d(4, 4);
    auto last = l.begin();
    ++last;
    ++last;                                       // 指向 1
    l.insert_after(last, d);                      // [2, 3, 1, 4]
    assert((values(l) == std::vector<int>{2, 3, 1, 4}));
    check_tail(l);

    const list& cl = l;
    std::vector<int> cv;
    for (auto it = cl.begin(); it != cl.end(); ++it)
    {
        cv.push_back(static_cast<const node&>(*it).value());
    }
    assert(cv == values(l));
    assert(value_of(cl.front()) == 2);

    list::const_iterator ci = l.begin();
    assert(ci == l.begin());
    assert(l.begin() == ci);

    // erase_after 只让后面的节点脱管，pos 本身仍纳管
    auto first = l.begin();                       // 2
    auto ret = l.erase_after(first);              // 摘掉 3
    assert(value_of(*ret) == 1);
    assert(c.is_released());
    assert(!b.is_released());
    assert((values(l) == std::vector<int>{2, 1, 4}));

    // 通过 before_begin 摘掉首节点
    ret = l.erase_after(l.before_begin());        // 摘掉 2
    assert(value_of(*ret) == 1);
    assert(b.is_released());
    assert((values(l) == std::vector<int>{1, 4}));

    // 摘掉尾节点
    ret = l.erase_after(l.begin());               // 摘掉 4
    assert(ret == l.end());
    assert(d.is_released());
    assert((values(l) == std::vector<int>{1}));
    check_tail(l);

    // 脱管的节点可以重新插入
    l.insert_after(l.begin(), b);                 // [1, 2]
    assert((values(l) == std::vector<int>{1, 2}));
    check_tail(l);

    // clear 让所有节点脱管，但节点仍存活
    l.clear();
    assert(l.empty());
    assert(a.is_released() && b.is_released());
    assert(l.begin() == l.end());
    check_tail(l);

    // 空链的 clear 是空操作
    l.clear();
    assert(l.empty());

    // emplace_* 与 insert_after / push_front 等价，只是名字对齐
    node e(5, 5);
    node f(6, 6);
    l.emplace_front(e);
    auto ei = l.emplace_after(l.begin(), f);
    assert(&*ei == &f);
    assert((values(l) == std::vector<int>{5, 6}));
    check_tail(l);
    l.clear();
    assert(e.is_released() && f.is_released());
}

static void test_erase_with_deleter()
{
    list l;
    l.push_front(*new node(1, 1));
    l.push_front(*new node(2, 2));
    l.push_front(*new node(3, 3));                // [3, 2, 1]
    assert(node::alive == 3);

    auto ret = l.erase_after(l.begin(), delete_node);   // 删除 2
    assert(value_of(*ret) == 1);
    assert(node::alive == 2);
    assert((values(l) == std::vector<int>{3, 1}));

    ret = l.erase_after(l.before_begin(), delete_node); // 删除 3
    assert(value_of(*ret) == 1);
    assert(node::alive == 1);

    ret = l.erase_after(l.before_begin(), delete_node); // 删除 1（尾节点）
    assert(ret == l.end());
    assert(node::alive == 0);
    assert(l.empty());
}

static void test_erase_range()
{
    {
        list l;
        node* ns[5];
        for (int i = 0; i != 5; ++i)
        {
            ns[i] = new node(i + 1, i);
        }
        for (int i = 4; i != 0; --i)
        {
            l.push_front(*ns[i]);
        }
        l.push_front(*ns[0]);                     // [1, 2, 3, 4, 5]
        assert((values(l) == std::vector<int>{1, 2, 3, 4, 5}));

        auto last = l.begin();
        std::advance(last, 3);                    // 指向 4
        l.erase_after(l.begin(), last);           // 摘掉 2、3
        assert(ns[1]->is_released() && ns[2]->is_released());
        assert(!ns[0]->is_released() && !ns[3]->is_released() && !ns[4]->is_released());
        assert((values(l) == std::vector<int>{1, 4, 5}));
        check_tail(l);

        l.clear(delete_node);                     // 删除 1、4、5
        assert(node::alive == 2);
        delete ns[1];
        delete ns[2];
        assert(node::alive == 0);
    }

    {
        list l;
        node* ns[5];
        for (int i = 0; i != 5; ++i)
        {
            ns[i] = new node(i + 1, i);
        }
        for (int i = 4; i >= 0; --i)
        {
            l.push_front(*ns[i]);
        }                                         // [1, 2, 3, 4, 5]
        assert(node::alive == 5);

        auto last = l.begin();
        std::advance(last, 3);                    // 指向 4
        auto ret = l.erase_after(l.begin(), last, delete_node); // 删除 2、3
        assert(ret == last);
        assert(node::alive == 3);
        assert((values(l) == std::vector<int>{1, 4, 5}));

        delete_remaining(l);
        assert(node::alive == 0);
    }
}

static void test_pop_clear()
{
    {
        node a(1, 1);
        node b(2, 2);
        list l;
        l.push_front(a);
        l.push_front(b);                          // [2, 1]
        l.pop_front();
        assert(b.is_released());
        assert(!a.is_released());
        l.pop_front();
        assert(a.is_released());
        assert(l.empty());
    }

    {
        list l;
        l.push_front(*new node(1, 1));
        l.push_front(*new node(2, 2));
        assert(node::alive == 2);
        l.pop_front(delete_node);
        assert(node::alive == 1);
        l.pop_front(delete_node);
        assert(node::alive == 0);
        assert(l.empty());
    }

    {
        node a(1, 1);
        node b(2, 2);
        list l;
        l.push_front(a);
        l.push_front(b);
        l.clear();
        assert(l.empty());
        assert(a.is_released() && b.is_released());
    }

    {
        list l;
        l.push_front(*new node(1, 1));
        l.push_front(*new node(2, 2));
        l.push_front(*new node(3, 3));
        assert(node::alive == 3);
        l.clear(delete_node);
        assert(node::alive == 0);
        assert(l.empty());
    }
}

static void test_destructor_detaches()
{
    node a(1, 1);
    node b(2, 2);
    {
        list l;
        l.push_front(a);
        l.push_front(b);
        assert(!a.is_released() && !b.is_released());
    }
    assert(a.is_released());
    assert(b.is_released());
}

static void test_remove_unique()
{
    {
        list l;
        for (int i = 1; i != 7; ++i)
        {
            l.push_front(*new node(i, i));
        }                                         // [6, 5, 4, 3, 2, 1]
        assert(node::alive == 6);

        auto removed = l.remove_if(even, delete_node);
        assert(removed == 3);
        assert((values(l) == std::vector<int>{5, 3, 1}));
        assert(node::alive == 3);

        delete_remaining(l);
        assert(node::alive == 0);
    }

    {
        list l;
        for (int v : { 1, 1, 2, 2, 2, 3 })
        {
            l.push_front(*new node(v, v));
        }                                         // [3, 2, 2, 2, 1, 1]
        assert(node::alive == 6);

        auto removed = l.unique(equal, delete_node);
        assert(removed == 3);
        assert((values(l) == std::vector<int>{3, 2, 1}));
        assert(node::alive == 3);

        delete_remaining(l);
        assert(node::alive == 0);
    }

    {
        // 无删除器版本：只脱管，节点仍存活
        list l;
        node* ns[6];
        for (int i = 0; i != 6; ++i)
        {
            ns[i] = new node(i + 1, i);
            l.push_front(*ns[i]);
        }                                         // [6, 5, 4, 3, 2, 1]

        auto removed = l.remove_if(even);
        assert(removed == 3);
        assert((values(l) == std::vector<int>{5, 3, 1}));
        for (int i = 0; i != 6; ++i)
        {
            assert(ns[i]->is_released() == (i % 2 == 1));
        }
        assert(node::alive == 6);

        l.clear();
        for (auto* p : ns)
        {
            delete p;
        }
        assert(node::alive == 0);
    }

    {
        list l;
        node* ns[6];
        int vals[6] = { 1, 1, 2, 2, 2, 3 };
        for (int i = 0; i != 6; ++i)
        {
            ns[i] = new node(vals[i], i);
            l.push_front(*ns[i]);
        }                                         // [3, 2, 2, 2, 1, 1]

        auto removed = l.unique(equal);
        assert(removed == 3);
        assert((values(l) == std::vector<int>{3, 2, 1}));
        assert(node::alive == 6);

        l.clear();
        for (auto* p : ns)
        {
            delete p;
        }
        assert(node::alive == 0);
    }

    {
        list empty;
        assert(empty.remove_if(even) == 0);
        assert(empty.unique(equal) == 0);
    }
}

static void test_sort()
{
    {
        list l;
        int vals[5] = { 3, 1, 2, 2, 3 };
        for (int i = 0; i != 5; ++i)
        {
            l.push_front(*new node(vals[i], i));
        }                                         // [3(id4), 2(id3), 2(id2), 1(id1), 3(id0)]
        l.sort(less);
        assert((values(l) == std::vector<int>{1, 2, 2, 3, 3}));
        assert((ids(l) == std::vector<int>{1, 3, 2, 4, 0}));
        check_tail(l);

        delete_remaining(l);
        assert(node::alive == 0);
    }

    {
        list empty;
        empty.sort(less);
        assert(empty.empty());

        list one;
        one.push_front(*new node(7, 7));
        one.sort(less);
        assert(value_of(one.front()) == 7);
        delete_remaining(one);
        assert(node::alive == 0);
    }

    {
        // 比较器抛异常：节点不丢，链表仍完整
        list l;
        for (int v : { 5, 1, 4, 2, 3 })
        {
            l.push_front(*new node(v, v));
        }
        assert(node::alive == 5);

        struct boom {};
        int compares = 0;
        auto bad_less = [&](const base& a, const base& b)
        {
            if (++compares == 3)
            {
                throw boom{};
            }
            return value_of(a) < value_of(b);
        };

        bool threw = false;
        try
        {
            l.sort(bad_less);
        }
        catch (boom&)
        {
            threw = true;
        }
        assert(threw);
        assert(node::alive == 5);

        auto vals = values(l);
        std::sort(vals.begin(), vals.end());
        assert((vals == std::vector<int>{1, 2, 3, 4, 5}));

        delete_remaining(l);
        assert(node::alive == 0);
    }
}

static void test_merge()
{
    {
        list a;
        list b;
        a.push_front(*new node(5, 2));
        a.push_front(*new node(3, 1));
        a.push_front(*new node(1, 0));            // [1, 3, 5]
        b.push_front(*new node(4, 5));
        b.push_front(*new node(3, 4));
        b.push_front(*new node(2, 3));            // [2, 3, 4]

        a.merge(b, less);
        assert(b.empty());
        assert((values(a) == std::vector<int>{1, 2, 3, 3, 4, 5}));
        assert((ids(a) == std::vector<int>{0, 3, 1, 4, 5, 2}));
        check_tail(a);

        list c;
        c.push_front(*new node(0, 6));
        a.merge(std::move(c), less);
        assert(c.empty());
        assert((values(a) == std::vector<int>{0, 1, 2, 3, 3, 4, 5}));

        delete_remaining(a);
        assert(node::alive == 0);
    }

    {
        list empty;
        list b;
        b.push_front(*new node(1, 1));
        b.push_front(*new node(2, 2));
        empty.merge(b, less);
        assert(b.empty());
        assert((values(empty) == std::vector<int>{2, 1}));
        delete_remaining(empty);
        assert(node::alive == 0);
    }
}

static void test_splice()
{
    {
        list a;
        list b;
        node* a0 = new node(1, 0);
        node* a1 = new node(2, 1);
        node* b0 = new node(3, 2);
        node* b1 = new node(4, 3);
        a.push_front(*a1);
        a.push_front(*a0);                        // [1, 2]
        b.push_front(*b1);
        b.push_front(*b0);                        // [3, 4]

        a.splice_after(a.before_begin(), b);      // [3, 4, 1, 2]
        assert(b.empty());
        assert((values(a) == std::vector<int>{3, 4, 1, 2}));
        assert(!b0->is_released() && !b1->is_released());
        check_tail(a);

        a.splice_after(a.before_begin(), a);      // 自己拼自己：无操作
        assert((values(a) == std::vector<int>{3, 4, 1, 2}));

        delete_remaining(a);
        assert(node::alive == 0);
    }

    {
        list a;
        list b;
        a.push_front(*new node(2, 2));
        a.push_front(*new node(1, 1));            // [1, 2]
        b.push_front(*new node(5, 5));
        b.push_front(*new node(4, 4));
        b.push_front(*new node(3, 3));            // [3, 4, 5]

        a.splice_after(a.begin(), b, b.before_begin()); // 把 3 移到 1 后
        assert((values(a) == std::vector<int>{1, 3, 2}));
        assert((values(b) == std::vector<int>{4, 5}));
        check_tail(a);
        check_tail(b);

        a.splice_after(a.before_begin(), b, b.before_begin(), b.end());
        assert(b.empty());
        assert((values(a) == std::vector<int>{4, 5, 1, 3, 2}));
        check_tail(a);

        delete_remaining(a);
        assert(node::alive == 0);
    }

    {
        // 同一条链上搬移首节点到尾部
        list l;
        node* n1 = new node(1, 1);
        node* n2 = new node(2, 2);
        node* n3 = new node(3, 3);
        l.push_front(*n3);
        l.push_front(*n2);
        l.push_front(*n1);                        // [1, 2, 3]

        auto last = l.begin();
        ++last;
        ++last;                                   // 指向 3
        l.splice_after(last, l, l.before_begin()); // 1 移到 3 后 -> [2, 3, 1]
        assert((values(l) == std::vector<int>{2, 3, 1}));
        check_tail(l);

        delete_remaining(l);
        assert(node::alive == 0);
    }
}

static void test_reverse_swap()
{
    {
        list l;
        node* n1 = new node(1, 0);
        node* n2 = new node(2, 1);
        node* n3 = new node(3, 2);
        node* n4 = new node(4, 3);
        l.push_front(*n4);
        l.push_front(*n3);
        l.push_front(*n2);
        l.push_front(*n1);                        // [1, 2, 3, 4]

        auto keep = l.begin();                    // 指向 1
        l.reverse();
        assert((values(l) == std::vector<int>{4, 3, 2, 1}));
        assert(value_of(*keep) == 1);             // 节点没搬，迭代器仍指向同一个节点
        auto after = keep;
        ++after;
        assert(after == l.end());
        check_tail(l);

        l.reverse();
        assert((values(l) == std::vector<int>{1, 2, 3, 4}));
        check_tail(l);

        list one;
        one.push_front(*new node(7, 7));
        one.reverse();
        assert(value_of(one.front()) == 7);
        check_tail(one);
        delete_remaining(one);

        list empty;
        empty.reverse();
        assert(empty.empty());

        delete_remaining(l);
        assert(node::alive == 0);
    }

    {
        node a(1, 1);
        node b(2, 2);
        list x;
        list y;
        x.push_front(a);
        y.push_front(b);
        x.swap(y);
        assert(value_of(x.front()) == 2);
        assert(value_of(y.front()) == 1);
        x.clear();
        y.clear();
        assert(a.is_released() && b.is_released());
    }
}

static void test_fancy()
{
    static_assert(std::is_same_v<flist::node_ptr, fancy_ptr<fbase>>);
    static_assert(std::forward_iterator<flist::iterator>);
    static_assert(std::forward_iterator<flist::const_iterator>);

    fnode* a = new fnode(3, 0);
    fnode* b = new fnode(1, 1);
    fnode* c = new fnode(2, 2);

    flist l;
    l.push_front(*a);
    l.insert_after(l.before_begin(), *b);         // [1, 3]
    l.insert_after(l.begin(), *c);                // [1, 2, 3]
    assert((fvalues(l) == std::vector<int>{1, 2, 3}));
    check_tail_f(l);

    const flist& cl = l;
    int sum = 0;
    for (auto it = cl.begin(); it != cl.end(); ++it)
    {
        sum += static_cast<const fnode&>(*it).value();
    }
    assert(sum == 6);
    assert(static_cast<const fnode&>(cl.front()).value() == 1);

    flist::const_iterator ci = l.begin();
    assert(ci == l.begin());
    assert(l.begin() == ci);

    l.sort([](const fbase& x, const fbase& y)
        {
            return static_cast<const fnode&>(x).value() < static_cast<const fnode&>(y).value();
        });
    assert((fvalues(l) == std::vector<int>{1, 2, 3}));

    l.reverse();
    assert((fvalues(l) == std::vector<int>{3, 2, 1}));
    check_tail_f(l);

    flist m;
    fnode* d = new fnode(0, 3);
    m.push_front(*d);
    l.splice_after(l.before_begin(), m);
    assert(m.empty());
    assert((fvalues(l) == std::vector<int>{0, 3, 2, 1}));
    check_tail_f(l);

    auto del = [](fbase& n)
    {
        delete static_cast<fnode*>(&n);
    };
    auto ret = l.erase_after(l.begin(), del);     // 删除 3
    assert(static_cast<const fnode&>(*ret).value() == 2);
    assert((fvalues(l) == std::vector<int>{0, 2, 1}));

    l.pop_front(del);                             // 删除 0
    assert((fvalues(l) == std::vector<int>{2, 1}));

    flist n;
    fnode* e = new fnode(4, 4);
    fnode* f = new fnode(5, 5);
    n.push_front(*f);
    n.push_front(*e);                             // [4, 5]
    auto cut = l.replace_tail_after(l.begin(), n); // l = [2, 4, 5]，cut = [1]
    assert((fvalues(l) == std::vector<int>{2, 4, 5}));
    assert((fvalues(cut) == std::vector<int>{1}));
    assert(n.empty());
    check_tail_f(l);
    check_tail_f(cut);

    cut.clear(del);                               // 删除 1
    l.clear(del);                                 // 删除 2、4、5
    assert(l.empty());
    assert(fnode::alive == 0);

    // 析构链表时节点脱管
    fnode x(7, 7);
    {
        flist z;
        z.push_front(x);
        assert(!x.is_released());
    }
    assert(x.is_released());
}

static void test_cut_replace()
{
    {
        list l;
        node a(1, 1);
        node b(2, 2);
        node c(3, 3);
        node d(4, 4);
        node e(5, 5);
        auto pos = l.before_begin();
        pos = l.insert_after(pos, a);
        pos = l.insert_after(pos, b);
        pos = l.insert_after(pos, c);
        pos = l.insert_after(pos, d);
        pos = l.insert_after(pos, e);              // [1, 2, 3, 4, 5]

        auto at = l.begin();
        ++at;                                     // 指向 2
        auto cut = l.cut_after(at);
        assert((values(l) == std::vector<int>{1, 2}));
        assert((values(cut) == std::vector<int>{3, 4, 5}));
        assert(!c.is_released() && !d.is_released() && !e.is_released());
        check_tail(l);
        check_tail(cut);

        cut.clear();
        assert(c.is_released() && d.is_released() && e.is_released());
        l.clear();
        assert(a.is_released() && b.is_released());
    }

    {
        // pos 是 before_begin：整条链被拆走
        list l;
        node a(1, 1);
        node b(2, 2);
        l.push_front(b);
        l.push_front(a);                          // [1, 2]
        auto cut = l.cut_after(l.before_begin());
        assert(l.empty());
        assert((values(cut) == std::vector<int>{1, 2}));
        assert(!a.is_released() && !b.is_released());
        check_tail(cut);
        cut.clear();
        assert(a.is_released() && b.is_released());
    }

    {
        // pos 是尾节点：拆出空链
        list l;
        node a(1, 1);
        node b(2, 2);
        l.push_front(b);
        l.push_front(a);                          // [1, 2]
        auto pos = l.begin();
        ++pos;                                    // 指向 2
        auto cut = l.cut_after(pos);
        assert(cut.empty());
        assert((values(l) == std::vector<int>{1, 2}));
        check_tail(l);
        l.clear();
        assert(a.is_released() && b.is_released());
    }

    {
        list l;
        auto cut = l.cut_after(l.before_begin());
        assert(l.empty());
        assert(cut.empty());
    }
}

static void test_replace_tail_after()
{
    {
        list l;
        list other;
        node a(1, 1);
        node b(2, 2);
        node c(3, 3);
        node d(4, 4);
        node x(7, 7);
        node y(8, 8);
        l.push_front(d);
        l.push_front(c);
        l.push_front(b);
        l.push_front(a);                          // [1, 2, 3, 4]
        other.push_front(y);
        other.push_front(x);                      // [7, 8]

        auto pos = l.begin();
        ++pos;                                    // 指向 2
        auto cut = l.replace_tail_after(pos, other);
        assert((values(l) == std::vector<int>{1, 2, 7, 8}));
        assert((values(cut) == std::vector<int>{3, 4}));
        assert(other.empty());
        assert(!x.is_released() && !y.is_released());
        assert(!c.is_released() && !d.is_released());
        check_tail(l);
        check_tail(cut);

        cut.clear();
        assert(c.is_released() && d.is_released());
        l.clear();
        assert(a.is_released() && b.is_released() && x.is_released() && y.is_released());
    }

    {
        // other 是右值
        list l;
        list other;
        node a(1, 1);
        node b(2, 2);
        node c(3, 3);
        node x(8, 8);
        node y(9, 9);
        l.push_front(c);
        l.push_front(b);
        l.push_front(a);                          // [1, 2, 3]
        other.push_front(y);
        other.push_front(x);                      // [8, 9]

        auto cut = l.replace_tail_after(l.begin(), std::move(other));
        assert((values(l) == std::vector<int>{1, 8, 9}));
        assert((values(cut) == std::vector<int>{2, 3}));
        assert(other.empty());
        check_tail(l);
        check_tail(cut);

        cut.clear();
        l.clear();
        assert(a.is_released() && b.is_released() && c.is_released()
            && x.is_released() && y.is_released());
    }

    {
        // other 为空：只裁剪
        list l;
        list other;
        node a(1, 1);
        node b(2, 2);
        node c(3, 3);
        l.push_front(c);
        l.push_front(b);
        l.push_front(a);                          // [1, 2, 3]
        auto pos = l.begin();
        ++pos;                                    // 指向 2
        auto cut = l.replace_tail_after(pos, other);
        assert((values(l) == std::vector<int>{1, 2}));
        assert((values(cut) == std::vector<int>{3}));
        assert(other.empty());
        check_tail(l);
        check_tail(cut);
        cut.clear();
        l.clear();
        assert(a.is_released() && b.is_released() && c.is_released());
    }

    {
        // pos 是 before_begin：整条链被换掉
        list l;
        list other;
        node a(1, 1);
        node b(2, 2);
        node x(7, 7);
        node y(8, 8);
        l.push_front(b);
        l.push_front(a);                          // [1, 2]
        other.push_front(y);
        other.push_front(x);                      // [7, 8]

        auto cut = l.replace_tail_after(l.before_begin(), other);
        assert((values(l) == std::vector<int>{7, 8}));
        assert((values(cut) == std::vector<int>{1, 2}));
        assert(other.empty());
        check_tail(l);
        check_tail(cut);
        cut.clear();
        l.clear();
        assert(a.is_released() && b.is_released() && x.is_released() && y.is_released());
    }

    {
        // pos 是尾节点：只接到尾部
        list l;
        list other;
        node a(1, 1);
        node b(2, 2);
        node x(7, 7);
        l.push_front(b);
        l.push_front(a);                          // [1, 2]
        other.push_front(x);                      // [7]
        auto pos = l.begin();
        ++pos;                                    // 指向 2
        auto cut = l.replace_tail_after(pos, other);
        assert((values(l) == std::vector<int>{1, 2, 7}));
        assert(cut.empty());
        assert(other.empty());
        check_tail(l);
        l.clear();
        assert(a.is_released() && b.is_released() && x.is_released());
    }
}

static void test_move()
{
    static_assert(std::is_move_constructible_v<list>);
    static_assert(std::is_move_assignable_v<list>);
    static_assert(std::is_nothrow_move_constructible_v<list>);
    static_assert(std::is_nothrow_move_assignable_v<list>);
    static_assert(!std::is_copy_constructible_v<list>);
    static_assert(!std::is_copy_assignable_v<list>);

    {
        node a(1, 1);
        node b(2, 2);
        list l;
        l.push_front(b);
        l.push_front(a);                          // [1, 2]
        list moved(std::move(l));
        assert(l.empty());
        assert((values(moved) == std::vector<int>{1, 2}));
        assert(!a.is_released() && !b.is_released());
        check_tail(moved);
        moved.clear();
        assert(a.is_released() && b.is_released());
    }

    {
        node a(1, 1);
        node b(2, 2);
        node c(3, 3);
        list l;
        l.push_front(b);
        l.push_front(a);                          // [1, 2]
        list dst;
        dst.push_front(c);                        // [3]
        dst = std::move(l);
        assert(l.empty());
        assert((values(dst) == std::vector<int>{1, 2}));
        assert(c.is_released());                  // 移动赋值把目标原有节点脱管
        assert(!a.is_released() && !b.is_released());
        check_tail(dst);
        dst.clear();
        assert(a.is_released() && b.is_released());
    }
}

static void test_stress()
{
    constexpr int n = 1000;
    list l;
    std::vector<node*> ns;
    ns.reserve(n);
    for (int i = 0; i != n; ++i)
    {
        ns.push_back(new node((i * 37) % 100, i));
    }

    auto pos = l.before_begin();
    for (auto* p : ns)
    {
        pos = l.insert_after(pos, *p);           // 保持 i 递增的原始顺序
    }
    assert(std::distance(l.begin(), l.end()) == n);

    l.sort(less);
    assert(std::distance(l.begin(), l.end()) == n);
    check_tail(l);

    int prev_value = -1;
    int prev_id = -1;
    for (auto it = l.begin(); it != l.end(); ++it)
    {
        int v = value_of(*it);
        int id = as_node(*it).id;
        assert(v >= prev_value);
        if (v == prev_value)
        {
            assert(id > prev_id);                // 相等元素保持原始顺序
        }
        prev_value = v;
        prev_id = id;
    }

    l.reverse();
    assert(std::distance(l.begin(), l.end()) == n);
    check_tail(l);
    l.reverse();
    assert(std::distance(l.begin(), l.end()) == n);
    check_tail(l);

    delete_remaining(l);
    assert(node::alive == 0);
}

int main()
{
    test_basic();
    test_erase_with_deleter();
    test_erase_range();
    test_pop_clear();
    test_destructor_detaches();
    test_remove_unique();
    test_sort();
    test_merge();
    test_splice();
    test_cut_replace();
    test_replace_tail_after();
    test_move();
    test_reverse_swap();
    test_fancy();
    test_stress();

    std::puts("test_intrusive_forward_list: ok");
}

#pragma once

namespace CurrentThread
{
    extern __thread int t_cachedTid;

    void cachedTid();

    inline int tid()
    {
        //为什么用__builtin_expect, 这里的__builtin_expect是告诉编译器大多数情况下 t_cachedTid == 0是不成立的，即t_cachedTid != 0,帮助编译器优化
        // 返回值是 t_cachedTid == 0 
        if (__builtin_expect(t_cachedTid == 0, 0)){ //如果是0 说明还没有获取过当前线程的id
            cachedTid();
        }
        return t_cachedTid;
    }
}
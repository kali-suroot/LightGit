/*
 * miniz_export.h
 *
 * miniz 的导出/可见性宏定义。
 *
 * 说明：
 *   该文件在上游 miniz 源码库中由 amalgamate.sh 生成，git checkout 里并不存在，
 *   但 miniz.h 会无条件 #include "miniz_export.h"，因此这里补齐一份。
 *
 * LightGit 采用“静态链接”方式把 miniz 编进最终 EXE，不需要任何 DLL 导入/导出
 * 装饰，所以 MINIZ_EXPORT 在默认路径下被定义为空。若将来要单独把 miniz 编成
 * DLL，可定义 MINIZ_DLL（消费方）与 MINIZ_EXPORTING（编译 DLL 本身）。
 */
#ifndef MINIZ_EXPORT_H
#define MINIZ_EXPORT_H

#if defined(_WIN32)
    #if defined(MINIZ_DLL)
        /* 消费 DLL：导入；编译 DLL 本体：导出 */
        #if defined(MINIZ_EXPORTING)
            #define MINIZ_EXPORT __declspec(dllexport)
        #else
            #define MINIZ_EXPORT __declspec(dllimport)
        #endif
    #else
        /* 静态链接：无需任何装饰 */
        #define MINIZ_EXPORT
    #endif
#else
    /* 非 Windows（GCC/Clang）：需要时导出符号 */
    #if defined(MINIZ_EXPORTING)
        #define MINIZ_EXPORT __attribute__((visibility("default")))
    #else
        #define MINIZ_EXPORT
    #endif
#endif

#endif /* MINIZ_EXPORT_H */

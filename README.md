# DlIteratePhdrEnhance

**DlIteratePhdrEnhance**是一个Android中用于增加原生`dl_iterate_phdr()`方法的库。



## 一、特征

- 使用c语言实现。
- 支持和统一从Android 4.0-15所有版本的`dl_iterate_phdr()`能力。
- `dl_iterate_phdr()`统一返回pathname。
- `dl_iterate_phdr()`调用前都加上锁。
- `dl_iterate_phdr()`回调的当前进程dlpi_name，都改为`system/bin/app_process(64)`完整路径。
- `dl_iterate_phdrd()`的回调中都将包含Linker本身。



## 二、实现原理
[https://juejin.cn/post/7469439971366338612](https://juejin.cn/post/7469439971366338612)




## 三、快速开始

你可以参考[app](https://github.com/835127729/MapsVisitor/tree/main/app)中的示例。

### 1、在 build.gradle.kts 中增加依赖

```Kotlin
android{
	buildFeatures {        
    //1、声明可以进行原生依赖，具体参考https://developer.android.com/build/native-dependencies
    prefab = true
  }
}

dependencies {    
  	//2、依赖最新版本
    implementation("com.github.835127729:DlIteratePhdrEnhance:1.0.0")
}
```



### 2、在 CMakeLists.txt增加依赖

```Makefile
...
find_package(iterate_phdr_enhance REQUIRED CONFIG)
  
...
target_link_libraries(${CMAKE_PROJECT_NAME}
        # List libraries link to the target library
        iterate_phdr_enhance::iterate_phdr_enhance
)
```



### 3、增加打包选项

如果你是在一个 SDK 工程里使用 MapsVisitor，你可能需要避免把`libmaps_visitor.so` 打包到你的 AAR 里，以免 app 工程打包时遇到重复的 `libmaps_visitor.so` 文件。

```Kotlin
android {
    packagingOptions {
        excludes += listOf(
            "**/libdl_iterate_phdr_enhance.so",
        )
    }
}
```

另一方面, 如果你是在一个 APP 工程里使用 MapsVisitor，你可以需要增加一些选项，用来处理重复的 `libmaps_visitor.so` 文件引起的冲突。

```Kotlin
android {
    packagingOptions {
        pickFirsts += listOf(
            "**/libdl_iterate_phdr_enhance.so",
        )
    }
}
```



### 4、使用

```c++
#include "dl_iterate_phdr_enhance.h"

dl_iterate_phdr_enhance([](struct dl_phdr_info *info, size_t size, void *data) {
    LOGD(TAG, "info->dlpi_name: %s", info->dlpi_name);
    LOGD(TAG, "info->dlpi_phdr: %p", info->dlpi_phdr);
    LOGD(TAG, "info->dlpi_phnum: %d", info->dlpi_phnum);
    return 0;
}, nullptr);
```







## 四、许可证

MapsVisitor 使用 [MIT 许可证](https://github.com/bytedance/bhook/blob/main/LICENSE) 授权。
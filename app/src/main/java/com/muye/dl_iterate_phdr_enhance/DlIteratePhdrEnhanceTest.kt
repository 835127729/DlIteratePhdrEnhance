package com.muye.dl_iterate_phdr_enhance

import com.muye.elfloader.ElfLoader

object DlIteratePhdrEnhanceTest {

    init {
        ElfLoader.loadLibrary("test")
    }

    //原生dl_iterate_phdr遍历
    external fun system()
    //dl_iterate_phdr_enhanced遍历
    external fun enhanced()
    //dl_iterate_phdr_by_maps遍历
    external fun byMaps()
}
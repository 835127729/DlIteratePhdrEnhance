package com.muye.dl_iterate_phdr_enhance

import com.muye.elfloader.ElfLoader

object DlIteratePhdrEnhanceTest {

    init {
        ElfLoader.loadLibrary("test")
    }

    external fun iteratePhdr()
}
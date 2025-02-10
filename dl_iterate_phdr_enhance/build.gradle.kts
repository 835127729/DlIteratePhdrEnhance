plugins {
    alias(libs.plugins.android.library)
    alias(libs.plugins.kotlin.android)
    `maven-publish`
}

android {
    namespace = "com.muye.dl_iterate_phdr_enhance"
    compileSdk = 34

    defaultConfig {
        minSdk = 16

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        consumerProguardFiles("consumer-rules.pro")
        externalNativeBuild {
            cmake {
                arguments("-DANDROID_STL=none")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }
    externalNativeBuild {
        cmake {
            path("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }
    buildFeatures {
        prefabPublishing = true
        prefab = true
    }
    prefab {
        create("dl_iterate_phdr_enhance") {
            headers = "src/main/cpp/include"
        }
    }
    publishing {
        singleVariant("release") {
            withSourcesJar()
        }
    }
    ndkVersion = "23.2.8568313"
    packagingOptions {
        excludes += listOf(
            "**/libdl_iterate_phdr_enhance.so",
        )
    }
}

publishing {
    publications {
        register<MavenPublication>("release") {
            groupId = "com.muye"
            artifactId = "dl_iterate_phdr_enhance"
            version = "1.0.0"
            afterEvaluate {
                from(components["release"])
            }
        }

        repositories {
            maven {
                name = "localRepo"
                url = uri(layout.buildDirectory.dir("repo"))
            }
        }
    }
}

dependencies {
    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.junit)
    androidTestImplementation(libs.androidx.espresso.core)
    implementation(libs.mapsvisitor)
}
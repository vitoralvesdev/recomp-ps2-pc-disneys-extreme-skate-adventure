cmake_minimum_required(VERSION 3.21)
project(DisneyMusicRecovery LANGUAGES CXX)
add_executable(DisneyMusicRecover recover_music.cpp)
target_compile_features(DisneyMusicRecover PRIVATE cxx_std_17)

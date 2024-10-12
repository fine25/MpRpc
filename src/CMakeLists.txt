aux_source_directory(. SRC_LIST)
add_library(mprpc ${SRC_LIST}) 
#muduo是静态库，所以mprpc也要是静态库
target_link_libraries(mprpc muduo_net muduo_base pthread /usr/local/lib/libzookeeper_mt.so.2)
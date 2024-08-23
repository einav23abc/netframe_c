gcc ^
    example.c ^
    netframe/netframe_client.c ^
    netframe/netframe_server.c ^
    netframe/netframe_internal.c ^
    sockets/sockets.c ^
    threads/threads.c ^
    -o example.exe ^
    -l "Ws2_32" ^
    -include config.h
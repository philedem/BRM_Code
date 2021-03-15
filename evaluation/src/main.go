package main

// #cgo LDFLAGS: -lgmp
// #include "../../main.c"
// static void* allocArgv(int argc) {
//    return malloc(sizeof(char *) * argc);
// }
import ("C")


import (
	"fmt"
	"os"
	"unsafe"
)

// Make a struct for each init state R2 
// when generating 

func strlen(s string, c chan int) { 
	c <- len(s)
}

func main() {
	argv := os.Args
    argc := C.int(len(argv))
    c_argv := (*[0xfff]*C.char)(C.allocArgv(argc))
    defer C.free(unsafe.Pointer(c_argv))

    for i, arg := range argv {
        c_argv[i] = C.CString(arg)
        defer C.free(unsafe.Pointer(c_argv[i]))
    }
    C.mainrun(argc, (**C.char)(unsafe.Pointer(c_argv)))
	
	//C.mainrun()
	c := make(chan int)
	go strlen("Salutations", c)
	go strlen("World", c) 
	x, y := <-c, <-c
    fmt.Println(x, y, x+y)
}

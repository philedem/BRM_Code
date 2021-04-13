package main

import (
	"fmt"
	//"os"
	"os/exec"
	"log"
)

func main() {

	make := exec.Command("make")
    _, err := make.CombinedOutput()

	if err != nil {
		log.Fatalf("Build failed %s\n", err)
		return 
	}

	cmd := exec.Command("./main", "11", "32", "64", "1", "100", "100", "0", "16")
	err = cmd.Run()
	if err != nil {
		log.Fatalf("cmd.Run() failed with %s\n", err)
	}
	fmt.Println("Done")
}

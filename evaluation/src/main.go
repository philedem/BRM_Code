//############################################
//
// Evaluation program for assesing BRM_Code
//
//############################################


package main

// #cgo LDFLAGS: -lgmp
// #cgo CFLAGS: -DSHIFTOR -DINC_INSERT
// #include "brm.c"
import "C"

import (
	"fmt"
	"os"
	"os/exec"
	"strconv"
	"sync"
    "github.com/schollz/progressbar/v3"
)

func getCands(pol int, m int, k int, i1 int, i2 int, c chan string, wg *sync.WaitGroup, bar *progressbar.ProgressBar) {
	err := exec.Command("../../main", strconv.Itoa(pol), strconv.Itoa(m), strconv.Itoa(k), strconv.Itoa(i1), strconv.Itoa(i2)).Run()
    var ret string
    var s int
	//score := float64(m)/float64(k)
    if err != nil { // No match found
    	s = 1
    	//fmt.Println(err)
    } else {
    	s = 0
    	ret = fmt.Sprintf("%d,%d,%d", m, k, s)
    	c <- ret
    }

    bar.Add(1) 
    defer wg.Done()
}

func getCands2(pol int, m int, k int, i1 int, i2 int) {//, c chan string, wg *sync.WaitGroup, bar *progressbar.ProgressBar) {
//func getCands2(pol int, m int, k int, i1 int, i2 int, c chan string, wg *sync.WaitGroup, bar *progressbar.ProgressBar) {
    r :=  C.brm(C.int(pol), C.int(m), C.int(k), C.int(i1), C.int(i2))
    //var ret string

	if r == 0 {
		fmt.Println("YAHOO")
    	//ret = fmt.Sprintf("%d,%d,%d", m, k, r)
    	//c <- ret
	} else {
		fmt.Println("BOO")

	}

	//bar.Add(1)
	//defer wg.Done()
}

func getTotal(min int, max int) int {
	var t int
	for i := min; i < max; i++ {
		for j := 1; j < (i / 2); j++ {
			t++
		}
	}
	return t
}

func main() {
	pol := 16
	min_m := 10
	max_m := 60
	r1_init := 20012
	r2_init := 32022

	count := 0

	total := getTotal(min_m, max_m) // Get total amount of iterations
	bar := progressbar.Default(int64(total)) // Initialize progressbar

	var wg sync.WaitGroup 
	c := make(chan string, 1000)
	//var result []string 

	// Calculate the total number of jobs to be run
	//getCands2(pol, 30, 20, r1_init, r2_init) //, c, &wg, bar)

	fmt.Printf("Testing R1 = %d, R2 = %d, m = %d ... %d, k = 1 ... (m/2) which gives a total of %d runs.\n", r1_init, r2_init, min_m, max_m, total)


	f, err :=  os.OpenFile("output.txt", os.O_CREATE|os.O_RDWR|os.O_APPEND, 0666);
    if err != nil {
             panic(err)
    }
    defer f.Close()

	// Iterate with increasing search word lengths.
	for i := min_m; i < max_m; i++ {
		// Iterate through increasing error levels, k until we reach the current m / 2
		for j := 1; j < (i / 2); j++ {
			wg.Add(1)
			//go getCands(pol, i, j, r1_init, r2_init, c, &wg, bar)
			go getCands(pol, i, j, r1_init, r2_init, c, &wg, bar)
		}
	}

	wg.Wait()
	close(c)

	for res := range c {
	    f.WriteString(res+"\n")
	    count++
	}

	fmt.Printf("%d of %d potentially valid sets found.\n", count, total)

	// Check for collissions

}

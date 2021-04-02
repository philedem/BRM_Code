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
	"time"
	"os"
	"os/exec"
	"strconv"
	"sync"
	"math"
	"math/rand"
    "github.com/schollz/progressbar/v3"
)

func getCands3(pol int, m int, k int, i1 int, i2 int, c chan string, wg *sync.WaitGroup, bar *progressbar.ProgressBar) { // Not in use! For legacy use with old binary.
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

func getCandidates(pol int, m int, k int, i1 int, i2 int, c chan string, wg *sync.WaitGroup, bar *progressbar.ProgressBar) {
    r :=  C.brm(C.int(pol), C.int(m), C.int(k), C.int(i1), C.int(i2))
    var ret string

	if r == 0 { // Match found
    	ret = fmt.Sprintf("%d,%d,%d,%d", pol, m, k, r)
    	c <- ret
	} 

	bar.Add(1)
	defer wg.Done()
}

func getTotal(min int, max int, err_ratio int) int {
	var t int
	for i := min; i <= max; i++ {
		for j := 1; j <= (i / err_ratio); j++ {
			t++
		}
	}
	return t
}

func checkCollisions() {
	fmt.Println("test Colissions")
}

func main() {
	pol := 11													// LFSR Polynomial
	min_m := 10													// Minimal m-length for search word
	max_m := 30													// Maximal m-length for search word
	err_ratio := 3												// Error rate in the form ( m / err_ratio )
	s1 := rand.NewSource(time.Now().UnixNano())					// Create random source
    r1 := rand.New(s1)											// Get random seed
	r1_init := r1.Intn(int(math.Exp2(float64(pol))))			// Generate a random initial state for the clocking LFSR (R2)
	r2_init := r1.Intn(int(math.Exp2(float64(pol))))			// Generate a random initial state for the clocked LFSR (R1)
	count := 0													// Iteration counter
	scp_upload := 0												// SCP Upload to remote destination

	data_path := "./data"										// System path to store data files				
	fname := fmt.Sprintf("%s/%d_%d_%d_%d_%d_%d.log", 			// Logfile name
		data_path, pol, min_m, max_m, err_ratio, r1_init, r2_init)					
	if _, err := os.Stat(data_path); os.IsNotExist(err) {		// Check if path already exist
		os.Mkdir(data_path, 0666)								// Create folder if not 
	}

	if scp_upload == 1 {

	} else {

	}
	// fname := 

	total := getTotal(min_m, max_m, err_ratio) // Get total amount of iterations
	bar := progressbar.Default(int64(total)) // Initialize progressbar

	var wg sync.WaitGroup 
	c := make(chan string, 1000)
	//var result []string 

	//for u := 1; u <= 1; u++ {
	//	getCands2(pol, 60, 10, r1_init, r2_init) //, c, &wg, bar)
	//}
	

	fmt.Printf("Testing R1 = %d, R2 = %d, m = %d ... %d, k = 1 ... (m/%d) which gives a total of %d combinations.\n", r1_init, r2_init, min_m, max_m, err_ratio, total)


	f, err :=  os.OpenFile(fname, os.O_CREATE|os.O_RDWR|os.O_APPEND, 0666);
    if err != nil {
             panic(err)
    }
    defer f.Close()

	// Iterate with increasing search word lengths.
	for i := min_m; i <= max_m; i++ {
		// Iterate through increasing error levels, k until we reach the current m / 2
		for j := 1; j <= (i / 3); j++ {
			wg.Add(1)
			go getCandidates(pol, i, j, r1_init, r2_init, c, &wg, bar)
		}
	}

	wg.Wait()
	close(c)

	for res := range c {
	    f.WriteString(res+"\n")
	    count++
	}

	fmt.Printf("%d of %d potentially valid sets found.\n", count, total)

	// Find the (X) lowest relationship c1 (search text length) and c2 (errors allowed) and check them for collisions through a brute force test.
	// For every relationsship get the corresponding candidates file and check for collissions iteratively.
	// If no collissions register it as a W and move on the next relationship.

	//for i := min_m; i <= max_m; i++ {
	//	checkCollisions
	//}

	// Check for collissions


}

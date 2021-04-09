//############################################
//
// Evaluation program for assesing BRM_Code
//
//############################################


package main

// #cgo LDFLAGS: -lgmp -lpthread
// #cgo CFLAGS: -DSHIFTOR -DINC_INSERT
// #include "brm.c"
import "C"

import (
	"fmt"
	"time"
	"io"
	"os"
	"log"
	"encoding/csv"
	"strconv"
	"path/filepath"
	"math"
	"math/rand"
    "github.com/schollz/progressbar/v3"
)

func main() {
	deg := 11													// LFSR Polynomial
	max_pol := int(math.Exp2(float64(deg)))
	min_m := 32													// Minimal m-length for search word
	max_m := 32													// Maximal m-length for search word
	err_low := 10
	err_high := 1												// Error rate in the form ( m / err_ratio )
	rsource := rand.NewSource(time.Now().UnixNano())			// Create random source
    rseed := rand.New(rsource)									// Get random seed
	r1_init := rseed.Intn(max_pol)			// Generate a random initial state for the clocking LFSR (R1)
	r2_init := rseed.Intn(max_pol)			// Generate a random initial state for the clocked LFSR (R2)
	plaintext := 0												// Default 0 plaintext
	count := 0													// Iteration counter
	scp_upload := 0												// SCP Upload to remote destination
	data_path := "./data"										// Local system path to store data files
	var ret string			
	fname := fmt.Sprintf("%s/%d_%d_%d_%d_%d_%d_%d.log", 			// Logfile name
		data_path, deg, r1_init, r2_init, min_m, max_m, err_low, err_high)		
	
	if (deg != 11 && deg != 16) {
		fmt.Println("Error: Valid polynomial degrees are 11 and 16")
		return
	} 

	if _, err := os.Stat(data_path); os.IsNotExist(err) {		// Check if data folder already exist
		os.Mkdir(data_path, 0777)								// Create folder if not 
	}

	if scp_upload == 1 {

	} else {

	}


	total := getTotal(min_m, max_m, err_high, err_low) // Get total amount of iterations
	bar := progressbar.Default(int64(total)) // Initialize progressbar
	fmt.Printf("Testing R1 = %d, R2 = %d, m = %d ... %d, k = (m/%d) ... (m/%d), total of %d combinations.\n", r1_init, r2_init, min_m, max_m, err_low, err_high, total)
	start := time.Now()

	//candidates := make([]string, 0, max) // Slice to hold return values

	f, err :=  os.OpenFile(fname, os.O_CREATE|os.O_RDWR|os.O_APPEND, 0666); // File to hold return values for later statistics
    if err != nil {
             panic(err)
    }
    defer f.Close() // Close file when function ends


	for i := min_m; i <= max_m; i++ {
		// Create target ciphertext of length m
		cipher := C.BRM_Encrypt(C.int(deg), C.int(i), C.int(r1_init), C.int(r2_init), C.int(plaintext)) 
		bar.Add(1)
		// Iterate through increasing error levels, k until we reach the current m / 2
		for j := (i / err_low); j <= (i / err_high); j++ {
			search_start := time.Now()
			r :=  C.search(C.int(deg), C.int(i), C.int(j), C.int(r1_init), C.int(r2_init), C.int(plaintext), C.int(cipher))
			search_end := time.Since(search_start)
	
			if int(r) != 0 && int(r) < max_pol-1 { // if more than 0 and less than max candidates were found
				// CHEAT 1: Check to see if actual R2 initial state is within the set
				cname := fmt.Sprintf("%s/%d_%d_%d_%d_%d.cand", data_path, deg, r1_init, r2_init, i, j)
				if candPresent(cname, r2_init) != true { // If r2_init is not present in candidate set
					e := os.Remove(cname)	// Delete the file if not present (the set parameters are still logged in the .log file)
					if e != nil {
						log.Fatal(e)
					}
				}

				ret = fmt.Sprintf("%d,%d,%d,%d", i, j, r, search_end)
				//candidates[i] = j
				f.WriteString(ret+"\n")
				count++
			} 
		}
	}

	fmt.Printf("%d of %d potentially valid sets found.\n", count, total)
	elapsed := time.Since(start)
	fmt.Println(elapsed)

	// Now move on to validating sets

	fmt.Printf("\nValidating sets...\n")

	pattern := fmt.Sprintf("%s/%d_%d_%d_*_*.cand", data_path, deg, r1_init, r2_init)

	matches, err := filepath.Glob(pattern)
	if err != nil {
		fmt.Println(err)
	}

	for _, s := range matches {
	 	fmt.Println(s)
	}

	elapsed = time.Since(start)
	fmt.Println(elapsed)
	
	return

	// Now check for collisions





	// Find the (X) lowest relationship c1 (search text length) and c2 (errors allowed) and check them for collisions through a brute force test.
	// For every relationsship get the corresponding candidates file and check for collissions iteratively.
	// If no collissions register it as a W and move on the next relationship.

	// for i := min_m; i <= max_m; i++ {
	// 	checkCollisions
	// }	
	
	return

	// var wg sync.WaitGroup 
	// c := make(chan string, 1000)	

	// // Iterate with increasing search word lengths.
	// for i := min_m; i <= max_m; i++ {
	// 	// Iterate through increasing error levels, k until we reach the current m / 2
	// 	for j := 1; j <= (i / 3); j++ {
	// 		wg.Add(1)
	// 		go getCandidates(deg, i, j, r1_init, r2_init, int(cipher), c, &wg, bar)
			
	// 	}
	// }

	// wg.Wait()
	// close(c)

	// for res := range c {
	//     f.WriteString(res+"\n")
	//     count++
	// }
	
	// fmt.Printf("%d of %d potentially valid sets found.\n", count, total)

	// // At this point we have generated the candidate sets for the given R1 and R2 for all combinations of search word length (m) and errors allowed (m/err_ratio).
	// // Only the candidate sets that contain the actual R2 initial state The candidate sets have been stored in data/ as .cand files.
	
	// 

	// Check for collissions


}

// func getCandidates(deg int, m int, k int, i1 int, i2 int, cipher int, c chan string, wg *sync.WaitGroup, bar *progressbar.ProgressBar) {
//     r :=  C.search(C.int(deg), C.int(m), C.int(k), C.int(i1), C.int(i2), C.int(0), C.int(cipher))
//     var ret string

// 	if r > 0 { // Match found
//     	ret = fmt.Sprintf("%d,%d,%d,%d", deg, m, k, r)
//     	c <- ret
// 	} 

// 	bar.Add(1)
// 	defer wg.Done()
// }

func checkCollisions() {
	fmt.Println("test Collisions")
}

func candPresent(cname string, r2_init int) bool {
	csvfile, err := os.Open(cname)
	if err != nil {
		log.Fatalln("Couldn't open the csv file", err)
	}
	r := csv.NewReader(csvfile)
	for {
		// Read each record from csv
		record, err := r.Read()
		if err == io.EOF {
			break
		}
		if err != nil {
			log.Fatal(err)
		}
		r2, _ := strconv.Atoi(record[0])
		if r2 == r2_init {
			return true
		}
	}
	csvfile.Close()
	return false
}

func getTotal(min int, max int, err_high int, err_low int) int {
	var t int
	for i := min; i <= max; i++ {
		for j := (i / err_low); j <= (i / err_high); j++ {
			t++
		}
	}
	return t
}

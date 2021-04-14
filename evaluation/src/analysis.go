package main

import (
	"fmt"
	"strconv"
	"os/exec"
	"log"
	"math"
	"strings"
	"time"
	"os"
)

func main() {
	make_cmd := exec.Command("make")
    err := make_cmd.Run()
	if err != nil {
		log.Fatalf("FATAL ERROR: Build failed %s\n", err)
		return 
	}

	m := 32
	stop_m := 100
	n := m*2
	r1 := 100
	r2 := 100
	col_accept := 0
	deg := 11
	k := 5
	cpus := 16
	//res := 0
	var status string
	var logline string

	max := int(math.Exp2(float64(deg)))
	data_path := "./data"										// Local system path to store data files

	fname := fmt.Sprintf("%s/%d_%d_%d_%d_%d_%d.log", 			// Logfile name
		data_path, deg, r1, r2, m, n, col_accept)
	//loghead := fmt.Sprintf("m,n,k,status,candidates,time,cpus")
	//writeLog(loghead)
	n = m*2 
	for x := m; n < stop_m; n++ {
		bottom := 0
		var resmap = make([]int, x-1)
		i := x/3

		SeekErrors:
			for true {
				// Start at m/3 and move up or down accordingly from first run
				if resmap[i] == 1 {
					i++
					continue SeekErrors
				}
				search_start := time.Now()
				cmd := exec.Command("./main", strconv.Itoa(deg), strconv.Itoa(x), strconv.Itoa(n), strconv.Itoa(i), strconv.Itoa(r1), strconv.Itoa(r2), strconv.Itoa(col_accept), strconv.Itoa(cpus))
				duration := time.Since(search_start)
				b, _ := cmd.Output()

				s := strings.Split(string(b), ",")
				res, _ := strconv.Atoi(s[0])
				cands, _ := strconv.Atoi(s[1])

				if res >= 1 {
					status = "invalid_no_r2"
					fmt.Printf("[m=%d,n=%d,k=%d] FAILED: Set of %d/%d contains no actual R2STATE... \n", x, n, i, cands,max)
				} else if res == -1 {
					status = "invalid_zero_set"
					bottom = 1
					fmt.Printf("[m=%d,n=%d,k=%d] FAILED: Zero candidates... \n", x, n, i)
				} else if res == -2 {
					status = "invalid_full_set"
					fmt.Printf("[m=%d,n=%d,k=%d] FAILED: Too many candidates... \n",x, n,  i)
				} else if res == -3 {
					status = "invalid_collisions"
					fmt.Printf("[m=%d,n=%d,k=%d] FAILED: Set of %d/%d contains collisions... \n",x, n, i,cands,max)
				} else if res == 0 {
					status = "valid"
					fmt.Printf("[m=%d,n=%d,k=%d] SUCCESS: Set of %d/%d is valid!\n",x, n, i,cands,max)
					// If success on first run, save the parameters and decrease. Skip ahead to a set value when done
				}
				resmap[i] = 1
				logline = fmt.Sprintf("%d,%d,%d,%d,%s,%d,%d\n",x,n,i,cands,status,duration,cpus)
				//fmt.Printf("%s",logline)
				writeLog(fname, logline)
				// Change output to CSV
				// m,n,k, invalid_full_set/invalid_zero_set/invalid_no_r2/invalid_collisions/valid
				if res >= 0 && bottom == 0 {
					i--
				} else if res >= 1 {
					i++
				} else if res == -1 {
					i++
				} else if res == -2 {
					break
				} else if res == -3 {
					break
				}

			}

		k++
	}
	fmt.Println("Done")
}

func writeLog(fname string, logline string) {
	f, err :=  os.OpenFile(fname, os.O_CREATE|os.O_APPEND|os.O_WRONLY, 0666); // File to hold return values for later statistics
	if err != nil {
		panic(err)
	}
	if _, err := f.WriteString(logline); err != nil {
		panic(err)
	}
	f.Close()

}

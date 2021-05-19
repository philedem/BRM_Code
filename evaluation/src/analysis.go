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
	"io"
	"runtime"
	//"io/ioutil"
	"encoding/csv"
)

func main() {
	

	data_path := "./data"										// Local system path to store data files
	cpus := runtime.NumCPU()
	
	// Get available jobs
	for {
		jobfile := fmt.Sprintf("%s/jobs.txt",data_path)			// Avilable jobs
		ret_job := getJob(jobfile) //

		deg, _ := strconv.Atoi(strings.TrimSpace(ret_job[0]))
		m, _ := strconv.Atoi( strings.TrimSpace(ret_job[1]))
		n, _ := strconv.Atoi( strings.TrimSpace(ret_job[2]))
		k, _ := strconv.ParseFloat( strings.TrimSpace(ret_job[3]), 32)
		stop_m, _ := strconv.Atoi( strings.TrimSpace(ret_job[4]))
		r1, _ := strconv.Atoi( strings.TrimSpace(ret_job[5]))
		r2, _ := strconv.Atoi( strings.TrimSpace(ret_job[6]))
		col_accept, _ := strconv.Atoi( strings.TrimSpace(ret_job[7]))
		mode := strings.TrimSpace(ret_job[8])
		fmt.Println(mode)
		
		
		make_cmd := exec.Command("make")

		if mode == "and" {
			make_cmd = exec.Command("make","and")
		} else if mode == "or2" {
			make_cmd = exec.Command("make","or2")
		} else if mode == "or3" {
			make_cmd = exec.Command("make","or3")
		}

		err := make_cmd.Run()
		if err != nil {
			log.Fatalf("FATAL ERROR: Build failed %s\n", err)
		return 
		}

		n=2*m

		var status string
		var logline string
	
		max := int(math.Exp2(float64(deg)))
	
		fname := fmt.Sprintf("%s/%s_%d_%d_%d_%d_%d_%d.log", 			// Logfile name
			data_path, mode, deg, r1, r2, m, n, col_accept)
		
		
		if n < m*2 {
			fmt.Printf("Search text too short...\n")
			continue
		} 

		for m < stop_m {
			n=2*m
			bottom := 0
			var resmap = make([]int, m-1)
			i := m/k
	
			SeekErrors:
				for true {
					if resmap[i] == 1 {
						i++
						continue SeekErrors
					}
					search_start := time.Now()
					cmd := exec.Command("./main", strconv.Itoa(deg), strconv.Itoa(m), strconv.Itoa(n), strconv.Itoa(i), strconv.Itoa(r1), strconv.Itoa(r2), strconv.Itoa(col_accept), strconv.Itoa(cpus))
					b, _ := cmd.Output()
					duration := time.Since(search_start)
					s := strings.Split(string(b), ",")
					res, _ := strconv.Atoi(s[0])
					cands, _ := strconv.Atoi(s[1])
	
					if res >= 1 {
						status = "invalid_no_r2"
						fmt.Printf("[m=%d,n=%d,k=%d] FAILED: Set of %d/%d contains no actual R2STATE... \n", m, n, i, cands,max)
					} else if res == -1 {
						status = "invalid_zero_set"
						bottom = 1
						fmt.Printf("[m=%d,n=%d,k=%d] FAILED: Zero candidates... \n", m, n, i)
					} else if res == -2 {
						status = "invalid_full_set"
						fmt.Printf("[m=%d,n=%d,k=%d] FAILED: Too many candidates... \n",m, n,  i)
					} else if res == -3 {
						status = "invalid_collisions"
						fmt.Printf("[m=%d,n=%d,k=%d] FAILED: Set of %d/%d contains collisions... \n",m, n, i,cands,max)
					} else if res == 0 {
						status = "valid"
						fmt.Printf("[m=%d,n=%d,k=%d] SUCCESS: Set of %d/%d is valid!\n",m, n, i,cands,max)
						// If success on first run, save the parameters and decrease. Skip ahead to a set value when done
					}
					resmap[i] = 1
					logline = fmt.Sprintf("%d,%d,%d,%d,%s,%d,%d\n",m,n,i,cands,status,duration,cpus)

					writeLog(fname, logline)

					// m,n,k, invalid_full_set/invalid_zero_set/invalid_no_r2/invalid_collisions/valid
					if res >= 0 && bottom == 0 {
						i--
					} else if res == -2 && bottom == 0 {
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
	
			
			m=m+20
		}
	}
	fmt.Printf("Done\n")
	os.Exit(0)
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

func getJob(jobfile string) []string { //([string, string, string, string, string, string, string, string]){ // deg, m, n, k_factor, stop, r1, r2, col_accept 
	csvfile, err := os.Open(jobfile)
	if err != nil {
		log.Fatalln("Couldn't open the csv file", err)
	}
	os.Remove(jobfile)
	newfile, err  := os.OpenFile(jobfile, os.O_CREATE|os.O_EXCL|os.O_WRONLY, 0666)
	r := csv.NewReader(csvfile)
	i := 0
	
	ret_job, _ := r.Read()	
	if err == io.EOF {
		fmt.Printf("No more jobs, shutting down...\n")
		os.Exit(0)		
	}
	if err != nil {
		log.Fatal(err)
	}

	for {
		job, err := r.Read()
		if err == io.EOF {
			break
		}
		if err != nil {
			log.Fatal(err)
		}
		writer := csv.NewWriter(newfile)
		writer.Write(job)
		writer.Flush()
		i++
	}

	return ret_job

}

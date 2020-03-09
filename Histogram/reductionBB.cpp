// Data Structures and Algorithms II : Reduction and Memory Access
// Ruth Falconer  <r.falconer@abertay.ac.uk>
// Adapted from C++ AMP book http://ampbook.codeplex.com/license.

#include <chrono>
#include <iostream>
#include <iomanip>
#include <amp.h>
#include <time.h>
#include <string>
#include <numeric>

#define SIZE 1<<20 // same as 2^20
#define TS 32
// Need to access the concurrency libraries 
using namespace concurrency;

// Import things we need from the standard library
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::cout;
using std::endl;
using std::cin; //read in data

// Define the alias "the_clock" for the clock type we're going to use.
typedef std::chrono::steady_clock the_serial_clock;
typedef std::chrono::steady_clock the_amp_clock;

void report_accelerator(const accelerator a)
{
	const std::wstring bs[2] = { L"false", L"true" };
	std::wcout << ": " << a.description << " "
		<< endl << "       device_path                       = " << a.device_path
		<< endl << "       dedicated_memory                  = " << std::setprecision(4) << float(a.dedicated_memory) / (1024.0f * 1024.0f) << " Mb"
		<< endl << "       has_display                       = " << bs[a.has_display]
		<< endl << "       is_debug                          = " << bs[a.is_debug]
		<< endl << "       is_emulated                       = " << bs[a.is_emulated]
		<< endl << "       supports_double_precision         = " << bs[a.supports_double_precision]
		<< endl << "       supports_limited_double_precision = " << bs[a.supports_limited_double_precision]
		<< endl;
}
// List and select the accelerator to use
void list_accelerators()
{
	//get all accelerators available to us and store in a vector so we can extract details
	std::vector<accelerator> accls = accelerator::get_all();

	// iterates over all accelerators and print characteristics
	for (unsigned i = 0; i < accls.size(); i++)
	{
		accelerator a = accls[i];
		report_accelerator(a);
		//if ((a.dedicated_memory > 0) & (a.dedicated_memory < 0.5*(1024.0f * 1024.0f)))
		//accelerator::set_default(a.device_path);
	}

	accelerator::set_default(accls[2].device_path);
	accelerator acc = accelerator(accelerator::default_accelerator);
	std::wcout << " default acc = " << acc.description << endl;

} // list_accelerators

  // query if AMP accelerator exists on hardware
void query_AMP_support()
{
	std::vector<accelerator> accls = accelerator::get_all();
	if (accls.empty())
	{
		cout << "No accelerators found that are compatible with C++ AMP" << std::endl;
	}
	else
	{
		cout << "Accelerators found that are compatible with C++ AMP" << std::endl;
		list_accelerators();
	}
} // query_AMP_support

float vector_sum_amp_tiled(int element_count, std::vector<float>& source)
{
	// Using arrays as temporary memory.
	array<float, 1> arr_1(element_count, source.begin());
	array<float, 1> arr_2((element_count / TS) ? (element_count / TS) : 1);
	float total_time = 0;
	// array_views may be swapped after each iteration.
	array_view<float, 1> av_src(arr_1);
	array_view<float, 1> av_dst(arr_2);
	av_dst.discard_data();

	// It is wise to use exception handling here - AMP can fail for many reasons
	// and it useful to know why (e.g. using double precision when there is limited or no support)
	try
	{
		// Reduce using parallel_for_each as long as the sequence length
		// is evenly divisable to the number of threads in the tile.
		while ((element_count % TS) == 0)
		{
			parallel_for_each(extent<1>(element_count).tile<TS>(), [=](tiled_index<TS> tidx) restrict(amp)
			{
				// Use tile_static as a scratchpad memory.
				tile_static float tile_data[TS];

				unsigned int local_idx = tidx.local[0];
				tile_data[local_idx] = av_src[tidx.global];
				tidx.barrier.wait(); 

				//TO DO WRITE WITHIN TILE REDUCION HERE
				//SEE LECTURE NOTES AND WEB LINK FOR DIFFERENT EXAMPLES

				// Store the tile result in the global memory.
				if (local_idx == 0)
				{
					av_dst[tidx.tile] = tile_data[0];
				}
			});//end of parallel_for_each
			// Update the sequence length, swap source with destination.
			element_count /= TS;
			//std::cout << " element_count " << element_count << std::endl;
			std::swap(av_src, av_dst);
			av_dst.discard_data();
		}
		// Perform any remaining reduction on the CPU.
		std::vector<float> result(element_count);
		copy(av_src.section(0, element_count), result.begin());
		return std::accumulate(result.begin(), result.end(), 0.f);
	}
	catch (const Concurrency::runtime_exception& ex)
	{
		MessageBoxA(NULL, ex.what(), "Error", MB_ICONERROR);
	}
	return 0;
} // vector_sum_amp
//
//int main(int argc, char *argv[])
//{
//	query_AMP_support();
//
//	std::vector<int> v1(SIZE, 1);
//
//	unsigned element_count = SIZE;
//	float expected_result = 0.0f;
//	std::vector<float> source(element_count);
//	for (unsigned int i = 0; i < element_count; ++i)
//	{
//		// Element range is  (0 - 0.15) to avoid overflow or underflow
//		source[i] = (i & 0xf) * 0.01f;
//	}
//	
//	vector_sum_amp_tiled(element_count, source);
//
//	    //TO DO: VERIFY THE REDUCTION GIVES THE RIGHT ANSWER
//	
//		return 0;
//} // main
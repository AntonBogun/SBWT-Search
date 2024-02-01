import argparse

def process_files(input_file, output_file):
    with open(input_file, 'r') as infile, open(output_file, 'r') as outfile:
        inlines = infile.readlines()
        outlines = outfile.readlines()

        if len(inlines) != len(outlines):
            raise ValueError("Input and output files must have the same number of lines")

        for infile2, outfile2 in zip(inlines, outlines):
            process_file(infile2.strip(), outfile2.strip())

def process_file(input_file, output_file):
    # Open the input and output files
    with open(input_file, 'r') as infile, open(output_file, 'w') as outfile:
        # Read and process one line at a time
        for line in infile:
            process_line(line, outfile)

def process_line(line, outfile):
    numbers = list(map(int, line.split()))
    if len(numbers) > 1:
        sorted_numbers = [numbers[0]] + sorted(numbers[1:])#first number is index
        sorted_line = ' '.join(map(str, sorted_numbers))
        outfile.write(sorted_line + '\n')
    else:
        outfile.write(line)


def main():
    # Parse the command line arguments
    parser = argparse.ArgumentParser(description='Process some integers.')
    parser.add_argument('input_file', type=str, help='Input file name')
    parser.add_argument('output_file', type=str, help='Output file name')
    parser.add_argument('--list', action='store_true', help='Treat the files as lists of files, one on each line')

    args = parser.parse_args()

    if args.list:
        process_files(args.input_file, args.output_file, args.list)
    else:
        process_file(args.input_file, args.output_file)

if __name__ == '__main__':
    main()

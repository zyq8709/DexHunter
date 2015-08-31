#!/usr/bin/perl

opendir(DIR, ".") || die "can't opendir $some_dir: $!";
@traces = grep { /.*\.dmtrace\.data/ } readdir(DIR);

foreach (@traces)
{
    $input = $_;
    $input =~ s/\.data$//;

    $output = "$input.html";

    print("dmtracedump -h -p $input > $output\n");
    system("dmtracedump -h -p '$input' > '$output'");

}

closedir DIR;

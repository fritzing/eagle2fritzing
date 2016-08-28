#!/usr/bin/perl

# This is a simple script to automatically create the busses for internal connections when using vias for prototyping areas!
# run with "perl bus.pl < part.fpz > foo.txt" check foo.txt to make sure its what you want and place between the <buses></buses> xml


while (<>) {
  if (/connector id=[\"\']([^\"]+)[\"\'] .* name=[\"\']([^\"]+)[\"\']/) {
    #print "$1 $2\n";
    $conn = $1;
    $name = $2;
    push(@{$connectors{$name}}, $conn);
  }
}

$i = 1;

foreach my $group (keys %connectors) {
  #print "    <!-- $group --!>\n";
  #if (! ($group =~ /N\$/)) { next; }

  if ( scalar @{$connectors{$group}} <= 1) { next; }

  print " <bus id=\"internal", $i, "\">\n";
  foreach (@{$connectors{$group}}) {
    print "  <nodeMember connectorId=\"",$_,"\"/>\n";
    }
  print " </bus>\n";
  $i++;
}

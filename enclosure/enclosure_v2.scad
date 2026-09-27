// 23.5 x 51.5r x 12

module bottom() union() {
    difference() {
        union() {
            minkowski() {
                cube([23.5, 51.5, 14], center=true);
                cylinder(d=2, h=2,  center=true, $fn=20);
            }        
        }
        
        cube([23.5, 51.5, 14], center=true);
        translate([0,0,9.4]) cube([26, 55, 6], center=true);
        
        translate([7.5,-23,-5]) cube([2, 15, 11], center=true);
        translate([3.75,-23,-5]) cube([2, 15, 11], center=true);
        translate([0,-23,-5]) cube([2, 15, 11], center=true);
        translate([-3.75,-23,-5]) cube([2, 15, 11], center=true);
        translate([-7.5,-23,-5]) cube([2, 15, 11], center=true);
        
        translate([-12,15.25,4.91]) cube([5,10,4], center=true);
    }

    translate([9,-23.25,2.5]) difference() {
        translate([1,-1,0]) cube([4.5, 4.5, 3], center=true);
        cylinder(r=0.5, h=20, center=true, $fn=12);
    }        

    translate([-9,-23.25,2.5]) difference() {
        translate([-1,-1,0]) cube([4.5, 4.5, 3], center=true);
        cylinder(r=0.5, h=20, center=true, $fn=12);
    }    

    translate([0,24,-3.5]) difference() {
        cube([25, 3.5, 7], center=true);
        translate([0,,-2]) rotate(45, [1,0,0]) cube([25, 7, 9], center=true);
    }    

    translate([0,23.25,5]) difference() {
        cube([25, 5, 2.8], center=true);
        translate([9,0,0]) cylinder(r=0.5, h=20, center=true, $fn=12);
        translate([-9,0,0]) cylinder(r=0.5, h=20, center=true, $fn=12);
        cube([16, 6, 3], center=true);
    }   
   
    translate([0,32,-3]) {
        difference() {
            hull() {
                rotate(90, [0,1,0]) cylinder(r=5, h=5, center=true, $fn=64);
                translate([0,-3,0]) cube([5,5,10], center=true);
            }
            
            rotate(90, [0,1,0]) cylinder(r=3, h=7, center=true, $fn=64);
        }
    }    
}

 module hole() {
     translate([0, 0, 7.75]) cylinder(r=1.5, h=1, center=true, $fn=12);
     translate([0, 0, 6.95]) cylinder(r=0.8, h=5, center=true, $fn=12);
 }

 //translate([40,-26.75,0]) cube([10,21,15]);
//translate([40,0,0]) translate([0,-26.75,0]) cube([10,27.5,15]);
// translate([40,0,0]) translate([0,0.75,0]) cube([5,5,15], center=true);

module corner() cylinder(d=1, h=5, center=true, $fn=32);

module top() difference() {
    intersection() {
        
        difference() {
            minkowski() {
                cube([23.5, 51.5, 14], center=true);
                cylinder(d=2, h=2,  center=true, $fn=20);
            }        
            cube([23.5, 51.5, 13], center=true);
        }
        translate([0,0,7.4]) cube([26, 55, 2], center=true);
    }
    
    translate([9,-23.25,0]) hole();
    translate([-9,-23.25,0]) hole();
    
    translate([0,23.25,0]) {
        translate([9,0,0]) hole();
        translate([-9,0,0]) hole();
    }

    translate([0,0.75,0]) cube([5.5,5.5,18], center=true);
    
    translate([0, -8.75, 0]) cube([10,10,15], center=true);
    
    translate([0, -3.5, 6.95]) hull() {
        translate([-5,0,0]) corner();
        translate([5,0,0]) corner();
    };

    translate([0, -3.5, 6.95]) hull() {
        translate([-5,0,0]) corner();
        translate([-5,-10,0]) corner();
    };
    
    translate([0, -3.5, 6.95]) hull() {
        translate([5,0,0]) corner();
        translate([5,-10,0]) corner();
    };
}

module led(size, rim_y=4, rim_x=1) union() {
    translate([-size/2, -size/2, 0]) cube([size,size,3]);
    translate([(-size-rim_x)/2, (-size-rim_y)/2, 0]) cube([size+rim_x,size+rim_y,1]);
}

// bottom();
// rotate(180, [1,0,0]) translate([20,0,-8]) top();

led(5);

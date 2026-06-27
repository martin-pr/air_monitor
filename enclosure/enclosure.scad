union() {
    difference() {
        hull() {
            translate([-8.5,-24,9]) 
                cylinder(h=18, r=5, center=true, $fn=50);
            translate([8.5,-24,9]) 
                cylinder(h=18, r=5, center=true, $fn=50);

            translate([-8.5,24,9]) 
                cylinder(h=18, r=5, center=true, $fn=50);
            translate([8.5,24,9]) 
                cylinder(h=18, r=5, center=true, $fn=50);
        }
        
        difference() {
            hull() {
                translate([0,-4,9.5]) cube([24,45,16], center=true);

                translate([-8.5,24,9.5]) 
                    cylinder(h=16, r=3.5, center=true, $fn=50);
                translate([8.5,24,9.5]) 
                    cylinder(h=16, r=3.5, center=true, $fn=50);
            }
            translate([0,24,1]) cube([26,6,7], center=true);

            translate([-11,25.5,20]) 
                cylinder(h=18, r=2, center=true, $fn=50);
            translate([11,25.5,20]) 
                cylinder(h=18, r=2, center=true, $fn=50);

            translate([-11,-25.5,20]) 
                cylinder(h=18, r=2, center=true, $fn=50);
            translate([11,-25.5,20]) 
                cylinder(h=18, r=2, center=true, $fn=50);
        }
        translate([0,0,25]) cube([30,80,20], center=true);
        
        translate([0,24,0]) {
            translate([9,0,1]) {
                cylinder(r=0.75, h=4, $fn=10);
            }
            translate([-9,0,1]) {
                cylinder(r=0.75, h=4, $fn=10);
            }
        }

        translate([0,-25,0]) {
            translate([10.5,0,5]) cylinder(r=0.5, h=15, $fn=20);
            translate([-10.5,0,5]) cylinder(r=0.5, h=15, $fn=20);
        }

        translate([0,25,5]) {
            translate([10.5,0,5]) cylinder(r=0.5, h=15, $fn=20);
            translate([-10.5,0,5]) cylinder(r=0.5, h=15, $fn=20);
        }

        for(y=[7:4:19]) {    
            translate([0,y,6]) rotate(90, [0,1,0]) 
                cylinder(r=1.3, h=30, $fn=16, center=true);
        }    

        for(y=[5:4:21]) {    
            translate([0,y,9]) rotate(90, [0,1,0]) 
                cylinder(r=1.3, h=30, $fn=16, center=true);
        }    

        for(y=[7:4:19]) {    
            translate([0,y,12]) rotate(90, [0,1,0]) 
                cylinder(r=1.3, h=30, $fn=16, center=true);
        }   
       
        // 

        for(x=[-8:4:8]) {    
            translate([x,32,6]) rotate(90, [1,0,0]) 
                cylinder(r=1.3, h=10, $fn=16, center=true);
        }  

        for(x=[-6:4:6]) {    
            translate([x,32,9]) rotate(90, [1,0,0]) 
                cylinder(r=1.3, h=10, $fn=16, center=true);
        }    

        for(x=[-8:4:8]) {    
            translate([x,32,12]) rotate(90, [1,0,0]) 
                cylinder(r=1.3, h=10, $fn=16, center=true);
        }    
        
        translate([10,-19,11]) cube([5,10,5]);
        
    }

    translate([0,-33,5]) {
        difference() {
            hull() {
                rotate(90, [0,1,0]) cylinder(r=5, h=5, center=true, $fn=64);
                translate([0,3,0]) cube([5,5,10], center=true);
            }
            
            rotate(90, [0,1,0]) cylinder(r=3, h=7, center=true, $fn=64);
        }
    }

}


//////////////////

translate([30,0,0]) {
difference() {
    hull() {
        translate([-8.5,-24,1]) 
            cylinder(h=2, r=5, center=true, $fn=50);
        translate([8.5,-24,1]) 
            cylinder(h=2, r=5, center=true, $fn=50);

        translate([-8.5,24,1]) 
            cylinder(h=2, r=5, center=true, $fn=50);
        translate([8.5,24,1]) 
            cylinder(h=2, r=5, center=true, $fn=50);
    }
    
    difference() {
        hull() {
            translate([0,-22,9]) cube([24,2,16], center=true);

            translate([-8.5,24,9]) 
                cylinder(h=16, r=3.5, center=true, $fn=50);
            translate([8.5,24,9]) 
                cylinder(h=16, r=3.5, center=true, $fn=50);
        }

        translate([-11,25.5,9]) 
            cylinder(h=18, r=2.5, center=true, $fn=50);
        translate([11,25.5,9]) 
            cylinder(h=18, r=2.5, center=true, $fn=50);
    }
    
    translate([0,-25,0]) {
        translate([10.5,0,-0.1]) {
            cylinder(r=0.75, h=15, $fn=20);
            cylinder(r=1.5, h=1.5, $fn=20);
        }
        translate([-10.5,0,-0.1]) {
            cylinder(r=0.75, h=15, $fn=20);
            cylinder(r=1.5, h=1.5, $fn=20);
        }
    }

    translate([0,25,0]) {
        translate([10.5,0,-0.1]) {
            cylinder(r=0.75, h=15, $fn=20);
            cylinder(r=1.5, h=1.5, $fn=20);
        }
        translate([-10.5,0,-0.1]) {
            cylinder(r=0.75, h=15, $fn=20);
            cylinder(r=1.5, h=1.5, $fn=20);
        }
    }
    
    for(x=[-10:4:10]) {    
        translate([x,5,0]) hull() {
            cylinder(r=1, h=3, center=true, $fn=20);
            translate([0,16,0]) cylinder(r=1, h=3, center=true, $fn=20);
        }
    }  
}
    
}




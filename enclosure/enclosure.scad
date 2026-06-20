difference() {
    hull() {
        translate([-8.5,-28,9]) 
            cylinder(h=18, r=5, center=true, $fn=50);
        translate([8.5,-28,9]) 
            cylinder(h=18, r=5, center=true, $fn=50);

        translate([-8.5,28,9]) 
            cylinder(h=18, r=5, center=true, $fn=50);
        translate([8.5,28,9]) 
            cylinder(h=18, r=5, center=true, $fn=50);
    }
    
    difference() {
        hull() {
            translate([0,0,9.5]) cube([24,54,16], center=true);

            translate([-8.5,28,9.5]) 
                cylinder(h=16, r=3.5, center=true, $fn=50);
            translate([8.5,28,9.5]) 
                cylinder(h=16, r=3.5, center=true, $fn=50);
        }
        translate([0,24,0]) cube([26,6,7], center=true);

        translate([-11,30.5,9]) 
            cylinder(h=18, r=2.5, center=true, $fn=50);
        translate([11,30.5,9]) 
            cylinder(h=18, r=2.5, center=true, $fn=50);
    }
    translate([0,0,25]) cube([30,80,20], center=true);
    
    translate([0,24,0]) {
        translate([9,0,1]) {
            cylinder(r=0.75, h=10, $fn=10);
        }
        translate([-9,0,1]) {
            cylinder(r=0.75, h=10, $fn=10);
        }
    }

    translate([0,-30,0]) {
        translate([10.5,0,5]) cylinder(r=0.5, h=15, $fn=10);
        translate([-10.5,0,5]) cylinder(r=0.5, h=15, $fn=10);
    }

    translate([0,30,0]) {
        translate([10.5,0,5]) cylinder(r=0.5, h=15, $fn=10);
        translate([-10.5,0,5]) cylinder(r=0.5, h=15, $fn=10);
    }

    for(y=[11:4:23]) {    
        translate([0,y,5]) rotate(90, [0,1,0]) 
            cylinder(r=1.3, h=30, $fn=16, center=true);
    }    

    for(y=[9:4:25]) {    
        translate([0,y,8]) rotate(90, [0,1,0]) 
            cylinder(r=1.3, h=30, $fn=16, center=true);
    }    

    for(y=[11:4:23]) {    
        translate([0,y,11]) rotate(90, [0,1,0]) 
            cylinder(r=1.3, h=30, $fn=16, center=true);
    }   
   
    // 

    for(x=[-8:4:8]) {    
        translate([x,28,5]) rotate(90, [1,0,0]) 
            cylinder(r=1.3, h=10, $fn=16, center=true);
    }  

    for(x=[-10:4:10]) {    
        translate([x,28,8]) rotate(90, [1,0,0]) 
            cylinder(r=1.3, h=10, $fn=16, center=true);
    }    

    for(x=[-8:4:8]) {    
        translate([x,28,11]) rotate(90, [1,0,0]) 
            cylinder(r=1.3, h=10, $fn=16, center=true);
    }    
    
    //
  
    hull() {
        translate([-5,-30,8]) 
            cylinder(h=18, r=2, center=true, $fn=50);
        translate([5,-30,8]) 
            cylinder(h=18, r=2, center=true, $fn=50);
    }  
  
    translate([10,-19,11]) cube([5,10,5]);
}


//////////////////

translate([30,0,0]) {
difference() {
    hull() {
        translate([-8.5,-28,1.5]) 
            cylinder(h=3, r=5, center=true, $fn=50);
        translate([8.5,-28,1.5]) 
            cylinder(h=3, r=5, center=true, $fn=50);

        translate([-8.5,28,1.5]) 
            cylinder(h=3, r=5, center=true, $fn=50);
        translate([8.5,28,1.5]) 
            cylinder(h=3, r=5, center=true, $fn=50);
    }
    
    difference() {
        hull() {
            translate([0,0,9]) cube([24,54,16], center=true);

            translate([-8.5,28,9]) 
                cylinder(h=16, r=3.5, center=true, $fn=50);
            translate([8.5,28,9]) 
                cylinder(h=16, r=3.5, center=true, $fn=50);
        }

        translate([-11,30.5,9]) 
            cylinder(h=18, r=2.5, center=true, $fn=50);
        translate([11,30.5,9]) 
            cylinder(h=18, r=2.5, center=true, $fn=50);
    }
    
    translate([0,-30,0]) {
        translate([10.5,0,-1]) {
            cylinder(r=0.5, h=15, $fn=10);
            cylinder(r=1.5, h=2, $fn=10);
        }
        translate([-10.5,0,-1]) {
            cylinder(r=0.5, h=15, $fn=10);
            cylinder(r=1.5, h=2, $fn=10);
        }
    }

    translate([0,30,0]) {
        translate([10.5,0,-1]) {
            cylinder(r=0.5, h=15, $fn=10);
            cylinder(r=1.5, h=2, $fn=10);
        }
        translate([-10.5,0,-1]) {
            cylinder(r=0.5, h=15, $fn=10);
            cylinder(r=1.5, h=2, $fn=10);
        }
    }
    
    for(x=[-8:4:8]) for(y=[12:6:28]) {    
        translate([x,y,0]) 
            cylinder(r=1.3, h=10, $fn=16, center=true);
    }  

    for(x=[-6:4:6]) for(y=[9:6:28]) {    
        translate([x,y,0]) 
            cylinder(r=1.3, h=10, $fn=16, center=true);
    }  

    hull() {
        translate([-5,-30,8]) 
            cylinder(h=18, r=2, center=true, $fn=50);
        translate([5,-30,8]) 
            cylinder(h=18, r=2, center=true, $fn=50);
    }  

}
    
}




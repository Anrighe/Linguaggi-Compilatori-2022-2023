extern g(z);
def f(i) 
    for k=1, k<10, 1 in 
        if (k/2) >= 3 
            then 
                g(k) 
            else 
                g(k+1) 
        end 
    end;
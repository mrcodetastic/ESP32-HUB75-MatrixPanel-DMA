## Four Scan Panel pixel mapping test and tune  
This example is to help you set up pixel coordinate mapping on most common cases of Four_Scan_Panels such as 64x32 1/8 or 32x16 1/4. Please follow the steps below in sequence.

### 1. Run this code on a single panel
If the panel lines are filled sequentially, from left to right and from top to bottom, then you don't need to change the setting. 

### 2. If your panel filling in parts
Most often, the panel rows are filled in small segments, 4, 8 or 16 pixels (there are other values). This number is the pixel base of the panel. Substitute it into line 45 of the example and run the code again.

### 3. Wrong order of rows
At this point, your panel should already be filled with whole rows. If the top row is not the first to be filled, swap the expressions, marked in the comments as "1st, 3rd 'block' of rows" with "2nd, 4th 'block' of rows" one.

### 4. Wrong filling direction
If any block of rows is filled from right to left, change the formula according to the example shown in the lines 65-68 of the code.

### Conclusion
If your panel works correctly now - congratulations. But if not - it's okay. There are many types of different panels and it is impossible to foresee all the nuances. Create an issue and you will be helped!
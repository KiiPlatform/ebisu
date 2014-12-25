1. Get application information (excel) and resources from YouWill. (folder structure and file name pattern are defined in the excel template)

2. open the excel file, and make sure there's no ansi comma (,) in the content. Append a new column to spreadsheet, and set all cell value to ##END##. (This is becuase we will export the excel file to csv format, and usually there are \n in applicaiton description, so we add ##END## to help content parsing.)

3. Export the excel file to csv foramt
   (MS Excel might run into file encoding issues. So What I do is:import the excel file to Google Spreadsheet, and save as csv)

4. set RES_ROOT, and ROOT_FILE to reflect your local environment.

5. pulbish entry is the method: process_app_list()

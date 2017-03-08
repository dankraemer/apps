#set terminal ggi V1024x768
set terminal png
set title "Sample Plot"
set output 'measures.png'

plot 'data_measures_result'

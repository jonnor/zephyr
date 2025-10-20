

import numpy
import pandas

p = '/home/jon/projects/toothbrush/data/jonnor-brushing-1/har_record/2024-12-31T161239_other.npy'

p = '/home/jon/projects/toothbrush/data/xiao-test-2/har_record/0000-00-00T000232_brushing.npy'
p = '/home/jon/projects/toothbrush/data/xiao-test-2/har_record/0000-00-00T000212_brushing.npy'
a = numpy.load(p)

print(a.shape)

columns = ['gyro_x', 'gyro_y', 'gyro_z', 'acc_x', 'acc_y', 'acc_z']
df = pandas.DataFrame(a, columns=columns)
df = 2.0 * df / (2**15)
df['time'] = (1.0/50) * numpy.arange(len(df))
out_order =  ['time', 'acc_x', 'acc_y', 'acc_z', 'gyro_x', 'gyro_y', 'gyro_z', ]

print(df.head())
print(df.max())
df = df.round(6)

out = 'sensordata.csv'
df.to_csv(out, columns=out_order, index=False)
print('wrote', out)

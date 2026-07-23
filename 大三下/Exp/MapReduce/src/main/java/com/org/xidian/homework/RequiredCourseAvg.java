package com.org.xidian.homework;

import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.io.DoubleWritable;
import org.apache.hadoop.io.LongWritable;
import org.apache.hadoop.io.Text;
import org.apache.hadoop.mapreduce.Job;
import org.apache.hadoop.mapreduce.Mapper;
import org.apache.hadoop.mapreduce.Reducer;
import org.apache.hadoop.mapreduce.lib.input.FileInputFormat;
import org.apache.hadoop.mapreduce.lib.output.FileOutputFormat;

import java.io.IOException;

public class RequiredCourseAvg {
    public static class GradeMapper extends Mapper<LongWritable, Text, Text, DoubleWritable> {
        private final Text studentName = new Text();
        private final DoubleWritable scoreValue = new DoubleWritable();

        @Override
        protected void map(LongWritable key, Text value, Context context)
                throws IOException, InterruptedException {
            String line = value.toString().trim();
            if (line.isEmpty()) {
                return;
            }

            String[] fields = line.split(",");
            if (fields.length < 5) {
                return;
            }

            String name = fields[1].trim();
            String courseType = fields[3].trim();
            String scoreText = fields[4].trim();

            if (!"必修".equals(courseType)) {
                return;
            }

            try {
                double score = Double.parseDouble(scoreText);
                studentName.set(name);
                scoreValue.set(score);
                context.write(studentName, scoreValue);
            } catch (NumberFormatException ignored) {
                // Skip bad score rows.
            }
        }
    }

    public static class AvgReducer extends Reducer<Text, DoubleWritable, Text, DoubleWritable> {
        private final DoubleWritable avgValue = new DoubleWritable();

        @Override
        protected void reduce(Text key, Iterable<DoubleWritable> values, Context context)
                throws IOException, InterruptedException {
            double sum = 0.0;
            int count = 0;

            for (DoubleWritable value : values) {
                sum += value.get();
                count++;
            }

            if (count > 0) {
                avgValue.set(sum / count);
                context.write(key, avgValue);
            }
        }
    }

    public static void main(String[] args) throws Exception {
        if (args.length != 2) {
            System.err.println("Usage: RequiredCourseAvg <input> <output>");
            System.exit(2);
        }

        Configuration conf = new Configuration();
        Job job = Job.getInstance(conf, "required-course-average");
        job.setJarByClass(RequiredCourseAvg.class);

        job.setMapperClass(GradeMapper.class);
        job.setReducerClass(AvgReducer.class);
        job.setNumReduceTasks(1);

        job.setMapOutputKeyClass(Text.class);
        job.setMapOutputValueClass(DoubleWritable.class);
        job.setOutputKeyClass(Text.class);
        job.setOutputValueClass(DoubleWritable.class);

        FileInputFormat.setInputPaths(job, new Path(args[0]));
        FileOutputFormat.setOutputPath(job, new Path(args[1]));

        System.exit(job.waitForCompletion(true) ? 0 : 1);
    }
}

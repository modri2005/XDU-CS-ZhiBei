package com.org.xidian.homework;

import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.io.LongWritable;
import org.apache.hadoop.io.Text;
import org.apache.hadoop.mapreduce.Job;
import org.apache.hadoop.mapreduce.Mapper;
import org.apache.hadoop.mapreduce.Reducer;
import org.apache.hadoop.mapreduce.lib.input.FileInputFormat;
import org.apache.hadoop.mapreduce.lib.output.FileOutputFormat;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class GrandchildGrandparent {
    public static class RelationMapper extends Mapper<LongWritable, Text, Text, Text> {
        private final Text middlePerson = new Text();
        private final Text relationValue = new Text();

        @Override
        protected void map(LongWritable key, Text value, Context context)
                throws IOException, InterruptedException {
            String line = value.toString().trim();
            if (line.isEmpty()) {
                return;
            }

            String[] fields = line.split(",");
            if (fields.length < 2) {
                return;
            }

            String parent = fields[0].trim();
            String child = fields[1].trim();
            if (parent.isEmpty() || child.isEmpty()) {
                return;
            }

            middlePerson.set(child);
            relationValue.set("P:" + parent);
            context.write(middlePerson, relationValue);

            middlePerson.set(parent);
            relationValue.set("C:" + child);
            context.write(middlePerson, relationValue);
        }
    }

    public static class RelationReducer extends Reducer<Text, Text, Text, Text> {
        private final Text grandchildText = new Text();
        private final Text grandparentText = new Text();

        @Override
        protected void reduce(Text key, Iterable<Text> values, Context context)
                throws IOException, InterruptedException {
            List<String> parents = new ArrayList<>();
            List<String> children = new ArrayList<>();

            for (Text value : values) {
                String relation = value.toString();
                if (relation.startsWith("P:")) {
                    parents.add(relation.substring(2));
                } else if (relation.startsWith("C:")) {
                    children.add(relation.substring(2));
                }
            }

            for (String child : children) {
                for (String parent : parents) {
                    grandchildText.set(child);
                    grandparentText.set(parent);
                    context.write(grandchildText, grandparentText);
                }
            }
        }
    }

    public static void main(String[] args) throws Exception {
        if (args.length != 2) {
            System.err.println("Usage: GrandchildGrandparent <input> <output>");
            System.exit(2);
        }

        Configuration conf = new Configuration();
        Job job = Job.getInstance(conf, "grandchild-grandparent");
        job.setJarByClass(GrandchildGrandparent.class);

        job.setMapperClass(RelationMapper.class);
        job.setReducerClass(RelationReducer.class);
        job.setNumReduceTasks(1);

        job.setMapOutputKeyClass(Text.class);
        job.setMapOutputValueClass(Text.class);
        job.setOutputKeyClass(Text.class);
        job.setOutputValueClass(Text.class);

        FileInputFormat.setInputPaths(job, new Path(args[0]));
        FileOutputFormat.setOutputPath(job, new Path(args[1]));

        System.exit(job.waitForCompletion(true) ? 0 : 1);
    }
}

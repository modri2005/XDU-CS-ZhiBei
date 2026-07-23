package com.org.xidian.spark;

import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.Path;
import org.apache.spark.SparkConf;
import org.apache.spark.api.java.JavaPairRDD;
import org.apache.spark.api.java.JavaRDD;
import org.apache.spark.api.java.JavaSparkContext;
import org.apache.spark.api.java.Optional;

import scala.Tuple2;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

public class Social3HopInfluence {
    private static final String SEP = "\t";

    public static void main(String[] args) throws Exception {
        if (args.length < 2 || args.length > 4) {
            System.err.println("Usage: Social3HopInfluence <input> <output> [topK] [useCache]");
            System.err.println("Example: spark-submit --class com.org.xidian.spark.Social3HopInfluence target/SparkHomework.jar /user/sandbox/spark-homework/input/follows.txt /user/sandbox/spark-homework/output 5 true");
            System.exit(2);
        }

        String input = args[0];
        String output = args[1];
        int topK = args.length >= 3 ? Integer.parseInt(args[2]) : 5;
        boolean useCache = args.length < 4 || Boolean.parseBoolean(args[3]);

        SparkConf conf = new SparkConf().setAppName("Social3HopInfluence");
        JavaSparkContext sc = new JavaSparkContext(conf);
        sc.setLogLevel("WARN");

        deleteIfExists(sc, output);

        JavaRDD<String> lines = sc.textFile(input);

        JavaPairRDD<String, String> edges = lines.flatMapToPair(line -> {
            List<Tuple2<String, String>> result = new ArrayList<>();
            String trimmed = line.trim();
            if (trimmed.isEmpty() || trimmed.startsWith("#")) {
                return result.iterator();
            }

            String[] fields = trimmed.split("\\s+");
            if (fields.length >= 2) {
                String from = fields[0].trim();
                String to = fields[1].trim();
                if (!from.isEmpty() && !to.isEmpty()) {
                    result.add(new Tuple2<>(from, to));
                }
            }
            return result.iterator();
        }).distinct();

        if (useCache) {
            edges = edges.cache();
        }

        JavaPairRDD<String, Iterable<String>> adjacency = edges.groupByKey();
        if (useCache) {
            adjacency = adjacency.cache();
        }

        JavaPairRDD<String, String> hop1 = edges;
        JavaPairRDD<String, String> hop2 = nextHop(hop1, edges).distinct();
        JavaPairRDD<String, String> hop3 = nextHop(hop2, edges).distinct();

        if (useCache) {
            hop1 = hop1.cache();
            hop2 = hop2.cache();
            hop3 = hop3.cache();
        }

        JavaPairRDD<String, String> coverage = hop1.union(hop2)
                .union(hop3)
                .filter(pair -> !pair._1.equals(pair._2))
                .distinct();

        if (useCache) {
            coverage = coverage.cache();
        }

        JavaPairRDD<String, Integer> nonZeroCounts = coverage
                .mapToPair(pair -> new Tuple2<>(pair._1, 1))
                .reduceByKey(Integer::sum);

        JavaRDD<String> allUsers = edges.keys().union(edges.values()).distinct();
        JavaPairRDD<String, Integer> zeroCounts = allUsers.mapToPair(user -> new Tuple2<>(user, 0));
        JavaPairRDD<String, Integer> counts = zeroCounts.leftOuterJoin(nonZeroCounts)
                .mapToPair(pair -> new Tuple2<>(pair._1, pair._2._2.orElse(pair._2._1)));

        if (useCache) {
            counts = counts.cache();
        }

        long edgeCount = edges.count();
        long userCount = allUsers.count();
        long hop1Count = hop1.count();
        long hop2Count = hop2.count();
        long hop3Count = hop3.count();
        long coverageCount = coverage.count();

        saveAdjacency(adjacency, output + "/adjacency");
        savePairs(hop1, output + "/hop1");
        savePairs(hop2, output + "/hop2");
        savePairs(hop3, output + "/hop3");
        savePairs(coverage, output + "/coverage_pairs");

        List<Tuple2<String, Integer>> sortedCounts = new ArrayList<>(counts.collect());
        sortedCounts.sort((a, b) -> {
            int countCompare = Integer.compare(b._2, a._2);
            if (countCompare != 0) {
                return countCompare;
            }
            return a._1.compareTo(b._1);
        });

        List<String> countLines = new ArrayList<>();
        for (Tuple2<String, Integer> item : sortedCounts) {
            countLines.add(item._1 + SEP + item._2);
        }
        sc.parallelize(countLines, 1).saveAsTextFile(output + "/counts");

        int limit = Math.min(topK, countLines.size());
        sc.parallelize(countLines.subList(0, limit), 1).saveAsTextFile(output + "/topk");

        List<String> summary = Arrays.asList(
                "input=" + input,
                "output=" + output,
                "topK=" + topK,
                "useCache=" + useCache,
                "edgeCount=" + edgeCount,
                "userCount=" + userCount,
                "hop1PairCount=" + hop1Count,
                "hop2PairCount=" + hop2Count,
                "hop3PairCount=" + hop3Count,
                "uniqueCoveragePairCount=" + coverageCount
        );
        sc.parallelize(summary, 1).saveAsTextFile(output + "/summary");

        sc.close();
    }

    private static JavaPairRDD<String, String> nextHop(
            JavaPairRDD<String, String> currentPaths,
            JavaPairRDD<String, String> edges) {
        JavaPairRDD<String, String> keyedByMiddle = currentPaths
                .mapToPair(path -> new Tuple2<>(path._2, path._1));

        return keyedByMiddle.join(edges)
                .mapToPair(joined -> {
                    String start = joined._2._1;
                    String next = joined._2._2;
                    return new Tuple2<>(start, next);
                });
    }

    private static void savePairs(JavaPairRDD<String, String> pairs, String outputPath) {
        pairs.map(pair -> pair._1 + SEP + pair._2)
                .sortBy(line -> line, true, 1)
                .coalesce(1)
                .saveAsTextFile(outputPath);
    }

    private static void saveAdjacency(JavaPairRDD<String, Iterable<String>> adjacency, String outputPath) {
        adjacency.map(pair -> {
                    List<String> targets = new ArrayList<>();
                    for (String target : pair._2) {
                        targets.add(target);
                    }
                    Collections.sort(targets);
                    return pair._1 + SEP + String.join(",", targets);
                })
                .sortBy(line -> line, true, 1)
                .coalesce(1)
                .saveAsTextFile(outputPath);
    }

    private static void deleteIfExists(JavaSparkContext sc, String outputPath) throws Exception {
        Path path = new Path(outputPath);
        FileSystem fs = path.getFileSystem(sc.hadoopConfiguration());
        if (fs.exists(path)) {
            fs.delete(path, true);
        }
    }
}
